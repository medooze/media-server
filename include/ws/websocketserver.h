#ifndef _WebSocketServer_H_
#define _WebSocketServer_H_
#include "pthread.h"
#include <list>
#include "websockets.h"
#include "websocketconnection.h"
#include "utf8.h"
#include "cpim.h"


class WebSocketServer : public WebSocketConnection::Listener
{
public:
	class Handler
	{
	public:
		virtual void onWebSocketConnection(const HTTPRequest& request,WebSocket *ws) = 0;

	};
public:
	/** Constructors */
	WebSocketServer();
	~WebSocketServer();

	int Init(int port);
	int Start();
	void AddHandler(const std::string base,Handler* hnd);
	int Stop();
	int End();

	virtual void onUpgradeRequest(WebSocketConnection* conn);
	virtual void onDisconnected(WebSocketConnection* conn);
		
public:
        int Run();

private:
	typedef std::map<std::string,Handler *> Handlers;
	typedef std::list<WebSocketConnection*>	Connections;

        static void * run(void *par);

	void CreateConnection(int fd);
	void CleanZombies();

private:
	int inited;
	int serverPort;
	int server;

	Handlers handlers;
	Connections zombies;
	pthread_t serverThread;
	pthread_mutex_t	sessionMutex;
};



class TextEchoWebsocketHandler :
	public WebSocketServer::Handler,
	public WebSocket::Listener
{
public:
	virtual void onWebSocketConnection(const HTTPRequest& request,WebSocket *ws)
	{
		Debug("-onUpgradeRequest %s\n", request.GetRequestURI().c_str());
		ws->Accept(this);
	}
	virtual void onOpen(WebSocket *ws)
	{
		Debug("-onOpened\n");
	}
	virtual void onMessageStart(WebSocket *ws,WebSocket::MessageType type,const DWORD length)
	{
		Debug("-onMessageStart\n");
	}
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size)
	{
		//Parse UTF 8 
		UTF8Parser parser;
		
		//Parse ut8
		parser.Parse(data,size);
		
		//Get text
		std::wstring str = parser.GetWString();
		Debug("-onMessageData %ls\n",str.c_str());
		Dump(data,size);
		ws->SendMessage(str);
	}

	virtual void onWriteBufferEmpty(WebSocket *ws)
	{
	}

	virtual void onMessageEnd(WebSocket *ws)
	{
		Debug("-onMessageEnd\n");
	}
	virtual void onError(WebSocket *ws)
	{
		Debug("-onError\n");
	}
	virtual void onClose(WebSocket *ws)
	{
		Debug("-onClose\n");
	}
};

#endif
