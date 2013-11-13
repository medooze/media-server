/* 
 * File:   appmixer.h
 * Author: Sergio
 *
 * Created on 15 de enero de 2013, 15:38
 */

#ifndef APPMIXER_H
#define	APPMIXER_H
#include <string>
#include "config.h"
#include "video.h"
#include "logo.h"
#include "websockets.h"

class AppMixer : public WebSocket::Listener
{
public:
	AppMixer();

	int Init(VideoOutput *output);
	int DisplayImage(const char* filename);
	int WebsocketConnectRequest(int partId,WebSocket *ws,bool isPresenter);
	int End();

	virtual void onOpen(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type);
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size);
	virtual void onMessageEnd(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onClose(WebSocket *ws);
private:
	typedef std::map<int,WebSocket*> Viewers;
private:
	Logo		logo;
	VideoOutput*	output;
	WebSocket*	presenter;
	Viewers		viewers;
};

#endif	/* APPMIXER_H */

