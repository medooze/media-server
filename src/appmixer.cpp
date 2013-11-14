/* 
 * File:   appmixer.cpp
 * Author: Sergio
 * 
 * Created on 15 de enero de 2013, 15:38
 */

#include "appmixer.h"
#include "log.h"

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
		presenter->Close();
		//Nullify
		presenter = NULL;
	}
	for (Viewers::iterator it = viewers.begin(); it!=viewers.end(); ++it)
		it->second->Close();
}

int AppMixer::WebsocketConnectRequest(int partId,WebSocket *ws,bool isPresenter)
{
	Log("-WebsocketConnectRequest [partId:%d,isPresenter:%d]\n",partId,isPresenter);
	if (isPresenter)
	{
		if (presenter) presenter->Close();
		presenter = ws;
	} else {
		viewers[partId] = ws;
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

	if (ws==presenter)
		for (Viewers::iterator it = viewers.begin(); it!=viewers.end(); ++it)
			it->second->SendMessage(data,size);
	else if (presenter)
		presenter->SendMessage(data,size);
}

void AppMixer::onMessageEnd(WebSocket *ws)
{

}
void AppMixer::onClose(WebSocket *ws)
{
	if (ws==presenter)
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
