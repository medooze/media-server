/* 
 * File:   groupchat.h
 * Author: Sergio
 *
 * Created on 29 de julio de 2014, 16:10
 */

#ifndef GROUPCHAT_H
#define	GROUPCHAT_H

#include "config.h"
#include "tools.h"
#include "cpim.h"
#include "ws/websockets.h"
#include "use.h"
#include "wait.h"

class GroupChat :
	public WebSocket::Listener
{
private:
	class Client
	{
	public:
		Client(int id,GroupChat* server);
		virtual ~Client();
		int Connect(WebSocket* ws);
		int Disconnect();
		void MessageStart();
		void MessageData(const BYTE* data,DWORD size);
		void MessageEnd();
		int GetId(){return id;}
		int Send(const CPIMMessage &msg);
	private:
		int id;
		WebSocket* ws;
		GroupChat* server;
		Mutex lock;
		BYTE buffer[65535];
		DWORD length;
	};
public:
	
	GroupChat(std::wstring tag);
	~GroupChat();
	
	bool Init();
	int WebsocketConnectRequest(int partId,WebSocket *ws);
	bool SendMessage(int from, int to,std::wstring text);
	bool End();
	
	virtual void onOpen(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type, const DWORD length);
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size);
	virtual void onMessageEnd(WebSocket *ws);
	virtual void onWriteBufferEmpty(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onClose(WebSocket *ws);
	
	int Disconnect(WebSocket *socket);
	
protected:
	bool Send(int from, int to, MIMEWrapper *mime);
	void onIncomingMessage(Client* client,CPIMMessage *msg);
	
private:
	typedef std::map<int,Client*> Clients;
private:
	Clients clients;
	Use use;
	std::wstring tag;
};

#endif	/* GROUPCHAT_H */

