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

/*
No. of bytes Type [Value] Description
1 U8 3 message-type
1 U8 incremental
2 U16 x-position
2 U16 y-position
2 U16 width
2 U16 height
*/
BYTE framebufferUpdateRequest[10] = { 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff };
BYTE setPixetFormat[20] = { 0x00  ,0x00  ,0x00  ,0x00  ,0x20  ,0x18  ,0x00  ,0x01
				,0x00  ,0xff  ,0x00  ,0xff  ,0x00  ,0xff  ,0x10  ,0x0
				,0x00  ,0x00  ,0x00  ,0x00};
BYTE setEncodings[44] = {0x02  ,0x00  ,0x00  ,0x0a
			,0x00  ,0x00  ,0x00  ,0x01  ,0x00  ,0x00  ,0x00  ,0x07
			,0xff  ,0xff  ,0xfe  ,0xfc  ,0x00  ,0x00  ,0x00  ,0x05
			,0x00  ,0x00  ,0x00  ,0x02  ,0x00  ,0x00  ,0x00  ,0x00
			,0xff  ,0xff  ,0xff  ,0x21  ,0xff  ,0xff  ,0xff  ,0xe6
			,0xff  ,0xff  ,0xff  ,0x09  ,0xff  ,0xff  ,0xff  ,0x20};


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
		//LOck
		use.WaitUnusedAndLock();
		//Check if already presenting
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
		//Unlock
		use.Unlock();
	} else {
		//Lock
		use.IncUse();
		//Find 
		Viewers::iterator it = viewers.find(partId);
		//If already got a viewer for that aprticipant
		if (it!=viewers.end())
		{
			//Get viewer
			Viewer* viewer = it->second;
			//Close websocket
			viewer->ws->Close();
			//Delete it
			delete(viewer);	
			//Erase it
			viewers.erase(it);
		}
		//Create new viewer
		Viewer* viewer = new Viewer(partId,ws);
		//Set as ws user data
		ws->SetUserData(viewer);
		//add To map
		viewers[partId] = viewer;
		//Unlock
		use.DecUse();
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

	///Get user data
	void * user = ws->GetUserData();

	//Check if it is presenter
	if (user)
	{
	Log("-onMessageData user %p\n",ws);
	Dump(data,size);

		//Get viewer from user data
		Viewer* viewer = (Viewer*) user;
		//Lock
		use.IncUse();
		//Process data
		if (viewer->Process(data,size))
		{
			//Check if presenter is established
			if (presenter && presenter->CheckState(Presenter::Established))
			{
				//Send server ini
				viewer->SendServerIni(presenter->GetServerIniData(),presenter->GetServerIniSize());
				//Send FUR
				presenter->SendMessage((BYTE*)framebufferUpdateRequest,sizeof(framebufferUpdateRequest));
			}
		}
		//Unlock
		use.DecUse();
	} else if (ws==presenter->ws) {
	Log("-onMessageData presenter %p\n",ws);
	Dump(data,size);
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

	Log("<onMessageData\n");
}

void AppMixer::onMessageEnd(WebSocket *ws)
{
	//Get viewer
	Viewer* viewer = (Viewer*) ws->GetUserData();

	Log("-onMessageEndedi %s\n",viewer?"viewer":"presenter");

	if (viewer)
	{
		//Lock
		use.WaitUnusedAndLock();
		//Send to the presenter
		//if (presenter && presenter->ws && presenter->CheckState(Presenter::Established))
			//Send
			//presenter->SendMessage(viewer->GetMessage(),viewer->GetLength());
		//Unlock
		use.Unlock();
		//Cleare message
		viewer->ClearMessage();
	}
	else if (presenter && ws==presenter->ws && presenter->CheckState(Presenter::Established) && presenter->GetLength())
	{
		//We are established lock viewers
		use.WaitUnusedAndLock();
		//For each viewer
		for (Viewers::iterator it = viewers.begin(); it!=viewers.end(); ++it)
		{
			//Get viewer
			Viewer* viewer = it->second;
			//Check if it is waiting for the server ini
			if (viewer->CheckState(Viewer::Established) && (viewer->IsSafe() || presenter->IsFramebufferUpdate()))
			{
				Log("-Sending frame viewer:%p safe:%d update%d\n",viewer,viewer->IsSafe(),presenter->IsFramebufferUpdate());
				//Send server ini
				viewer->SendMessage(presenter->GetMessage(),presenter->GetLength());
				//We are safe
				viewer->SetSafe();
			}
		}
		//Unlock
		use.Unlock();
		//Cleare message
		presenter->ClearMessage();
	}
}

void AppMixer::onClose(WebSocket *ws)
{
	///Get user data
	void * user = ws->GetUserData();

	//Check if it is presenter
	if (user)
	{
		//Get viewer from user data
		Viewer* viewer = (Viewer*) user;
		//Lock
		use.WaitUnusedAndLock();
		//Erase it
		viewers.erase(viewer->GetId());
		//Delete it
		delete(viewer);
		//Unlcok
		use.Unlock();
	} else if (presenter && ws==presenter->ws) {
		//Lock
		use.WaitUnusedAndLock();
		//Delete presenter
		delete(presenter);
		//Remote presenter
		presenter = NULL;
		//Unlock
		use.Unlock();
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
	//NO length
	length = 0;
	//Not safe
	safe = 0;
}

void AppMixer::Viewer::StartHandshake()
{
	//Set state
	state = VersionSent;
	//Send
	ws->SendMessage((BYTE*)"RFB 003.008\n",12);
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
				//Get what is left in the buffer
				length += buffer.pop(message+length,buffer.length());
		}
	}
	return waitingForServeIni;
}

void  AppMixer::Viewer::SendServerIni(BYTE* data,DWORD size)
{
	//Move state
	state = Established;
	//Log
	Log("-VNC viewer established %p\n",this);
	//Send security
	ws->SendMessage(data,size);
}

void AppMixer::Viewer::SendMessage(const BYTE* data,DWORD size)
{
	//Send data
	ws->SendMessage(data,size);
}

void AppMixer::Presenter::SendMessage(const BYTE* data,DWORD size)
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
	//NO length
	length = 0;
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
					ws->SendMessage((BYTE*)"RFB 003.008\n",12);
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
					//Log
					Log("-VNC presenter established %p\n",this);
					//Send  pixet formats
					ws->SendMessage((BYTE*)setPixetFormat,sizeof(setPixetFormat));
					//Sned encodings
					ws->SendMessage((BYTE*)setEncodings,sizeof(setEncodings));
					//Send FUR
					ws->SendMessage((BYTE*)framebufferUpdateRequest,sizeof(framebufferUpdateRequest));
				
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
