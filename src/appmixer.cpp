/* 
 * File:   appmixer.cpp
 * Author: Sergio
 * 
 * Created on 15 de enero de 2013, 15:38
 */

#include <netdb.h>

#include "appmixer.h"
#include "log.h"
#include "fifo.h"
#include "use.h"

AppMixer::AppMixer()
{
	//No output
	output = NULL;
	//No presenter
	presenter = NULL;
}

int AppMixer::Init(VideoOutput* output)
{
	//Set output
	this->output = output;
}

int AppMixer::DisplayImage(const char* filename)
{
	Log("-DisplayImage [\"%s\"]\n",filename);

	//Load image
	if (!logo.Load(filename))
		//Error
		return Error("-Error loading file");

	//Check output
	if (!output)
		//Error
		return Error("-No output");

	//Set size
	output-> SetVideoSize(logo.GetWidth(),logo.GetHeight());
	//Set image
	output->NextFrame(logo.GetFrame());

	//Everything ok
	return true;
}

int AppMixer::End()
{
	//Reset output
	output = NULL;

	//Check if presenting
	if (presenter)
	{
		//End it
		presenter->ws->Close();
		//Delete presenter
		delete(presenter);
		//Nullify
		 presenter = NULL;
	}
	for (Viewers::iterator it = viewers.begin(); it!=viewers.end(); ++it)
		it->second->ws->Close();
}

int AppMixer::WebsocketConnectRequest(int partId,WebSocket *ws,bool isPresenter)
{
	Log("-WebsocketConnectRequest [partId:%d,isPresenter:%d]\n",partId,isPresenter);
	if (isPresenter)
	{
		if (presenter)
		{
			//Close websocket
			presenter->ws->Close();
			//Delete it
			delete(presenter);
		}
		//Create new presenter
		presenter = new Presenter(ws);
		//No user data
		ws->SetUserData(NULL);
	} else {
		//Create new viewer
		Viewer* viewer = new Viewer(partId,ws);
		//Set as ws user data
		ws->SetUserData(viewer);
		//add To map
		viewers[partId] = viewer;
		//Start handshake
		viewer->StartHandshake();
	}

	//Accept connection
	ws->Accept(this);
}

void AppMixer::onOpen(WebSocket *ws)
{

}

void AppMixer::onMessageStart(WebSocket *ws,const WebSocket::MessageType type)
{

}

void AppMixer::onMessageData(WebSocket *ws,const BYTE* data, const DWORD size)
{
	Log("-onMessageData %p\n",ws);
	Dump(data,size);

	///Get user data
	void * user = ws->GetUserData();

	//Check if it is presenter
	if (user)
	{

		//Get viewer from user data
		Viewer* viewer = (Viewer*) user;
		//Lock
		use.IncUse();
		//Process data
		if (viewer->Process(data,size))
		{
			//Check if presenter is established
			if (presenter && presenter->CheckState(Presenter::Established))
				//Send server ini
				viewer->SendServerIni(presenter->GetServerIniData(),presenter->GetServerIniSize());
		}
		//Unlock
		use.DecUse();
	} else if (ws==presenter->ws) {
		//Process it
		if (presenter->Process(data,size))
		{
			//We are established lock viewers 
			use.WaitUnusedAndLock();
			//For each viewer
			for (Viewers::iterator it = viewers.begin(); it!=viewers.end(); ++it)
			{
				//Get viewer
				Viewer* viewer= it->second;
				//Check if it is waiting for the server ini
				if (viewer->CheckState(Viewer::WaitingForServerIni))
					//Send server ini
					viewer->SendServerIni(presenter->GetServerIniData(),presenter->GetServerIniSize());
			}
			//Unlock
			use.Unlock();
		}

			
	}
}

void AppMixer::onMessageEnd(WebSocket *ws)
{
	if (presenter && ws==presenter->ws)
	{
		//We are established lock viewers
		use.WaitUnusedAndLock();
		//For each viewer
		for (Viewers::iterator it = viewers.begin(); it!=viewers.end(); ++it)
		{
			//Get viewer
			Viewer* viewer= it->second;
			//Check if it is waiting for the server ini
			if (viewer->CheckState(Viewer::WaitingForServerIni))
				//Send server ini
				viewer->SendMessage(presenter->GetMessage(),presenter->GetLength());
		}
		//Unlock
		use.Unlock();
		//Cleare message
		presenter->ClearMessage();
	}
}
void AppMixer::onClose(WebSocket *ws)
{
	if (ws==presenter->ws)
	{
		//Remote presenter
		presenter = NULL;
	} else {
		//Remove
	}
}

void AppMixer::onError(WebSocket *ws)
{

}

AppMixer::Viewer::Viewer(int id,WebSocket *ws)
{
	//Store id
	this->id = id;
	//Store ws
	this->ws = ws;
	//No state
	state = None;
}

void AppMixer::Viewer::StartHandshake()
{
	//Set state
	state = VersionSent;
	//Send
	ws->SendMessage((BYTE*)"RFB.003.008\n",12);
}

int AppMixer::Viewer::Process(const BYTE* data,DWORD size)
{
	int waitingForServeIni = 0;
	BYTE security[2]	= { 0x01, 0x01 };
	BYTE securityResult[4]	= { 0x00, 0x00, 0x00, 0x00 };

	//Append to buffer
	buffer.push(data,size);

	//Whiel we have size
	while(buffer.length())
	{
		//Depending on the state
		switch(state)
		{
			case VersionSent:
				//Check if enought data
				if (buffer.length()<12)
					//Exit
					return waitingForServeIni;
				//Consume it
				buffer.remove(12);
				//Move state
				state = SecuritySent;
				//Send security
				ws->SendMessage((BYTE*)security,sizeof(security));
				break;
			case SecuritySent:
				//Consume result
				buffer.remove(1);
				//Move state
				state = SecurityResultSent;
				//Send security
				ws->SendMessage((BYTE*)securityResult,sizeof(securityResult));
				break;
			case SecurityResultSent:
				//Consume client ini
				buffer.remove(1);
				//Move state
				state = WaitingForServerIni;
				//We are waiting for server ini
				waitingForServeIni = 1;
				break;
			default:
				return waitingForServeIni;
		}
	}
	return waitingForServeIni;
}

void  AppMixer::Viewer::SendServerIni(BYTE* data,DWORD size)
{
	//Move state
	state = Established;
	//Send security
	ws->SendMessage(data,size);
}

void AppMixer::Viewer::SendMessage(const BYTE* data,DWORD size)
{
	//Send data
	ws->SendMessage(data,size);
}

AppMixer::Presenter::Presenter(WebSocket *ws)
{
	//Store ws
	this->ws = ws;
	//Set empty state
	state = WaitingVersion;
}

int AppMixer::Presenter::Process(const BYTE* data,DWORD size)
{
	int established = 0;
	BYTE security[1]	= { 0x01 };
	BYTE clientIni[1]	= { 0x00 };

	//Append to buffer
	buffer.push(data,size);

	//If not in established state
	if (!CheckState(Established))
	{
		//While we have size
		while(buffer.length())
		{
			//Depending on the state
			switch(state)
			{
				case WaitingVersion:
					//Check if enought data
					if (buffer.length()<12)
						//Exit
						return established;
					//Consume it
					buffer.remove(12);
					//Move state
					state = WaitingSecurity;
					//Send security
					ws->SendMessage((BYTE*)"RFB.003.008\n",12);
					break;
				case WaitingSecurity:
				{
					//Get number of security types on message
					BYTE n = buffer.peek();
					//Check size
					if (buffer.length()<n+1)
						//Nothing yet
						return established;
					//Consume
					buffer.remove(n+1);
					//Move state
					state = WaitingSecurityResult;
					//Send security
					ws->SendMessage((BYTE*)security,sizeof(security));
					break;
				}
				case WaitingSecurityResult:
				{
					//Check if enought data
					if (buffer.length()<4)
						//Exit
						return established;
					//Consume it
					buffer.remove(4);
					//Move state
					state = WaitingServerIni;
					//Send client ini
					ws->SendMessage((BYTE*)clientIni,sizeof(clientIni));
					break;
				}
				case WaitingServerIni:
				{
					//Check if enought data for server ini without name
					if (buffer.length()<24)
						//Exit
						return established;
					//Peek server ini
					buffer.peek(serverIni,24);
					//Get name length
					DWORD len = get4(serverIni,20);
					//Check if enought data
					if (buffer.length()<24+len)
						//Exit
						return established;
					//Consume it
					buffer.remove(24+len);
					//Update name
					serverIni[24] = 'M';
					serverIni[25] = 'C';
					serverIni[26] = 'U';
					//Set lenght
					set4(serverIni,20,3);
					//Move state
					state = Established;
					//Get what is left in the buffer
					length = buffer.pop(message,buffer.length());
					//Just established
					established = 1;
					break;
				}
			}
		}
	} else {
		//Check data
		if (length+size>65535)
			//Error
			return 0;
		//Copy message
		memcpy(message+length,data,size);
		//Inc size
		length+=size;
	}
		
	//Return if established in this process
	return established;
}
