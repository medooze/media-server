#ifndef BFCP_H
#define	BFCP_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/BFCPAttribute.h"
#include "bfcp/messages.h"
#include "bfcp/attributes.h"
#include "bfcp/BFCPFloorControlServer.h"
#include "config.h"
#include "websockets.h"
#include "websocketserver.h"
#include "websocketconnection.h"
#include "amf.h"
#include <map>


class BFCP :
	public WebSocketServer::Handler,
	public WebSocket::Listener
{
public:
	static void Init();

	static BFCP& getInstance()
        {
            static BFCP   instance;
            return instance;
        }

private:
	BFCP();

public:
	// Called by the MCU instance.
	BFCPFloorControlServer* CreateConference(int conferenceId, BFCPFloorControlServer::Listener *listener);
	bool RemoveConference(BFCPFloorControlServer* conference);
	bool RemoveConference(int conferenceId);
	bool ConnectTransport(WebSocket *ws, BFCPFloorControlServer* conference, int userId);

private:
	BFCPFloorControlServer* GetConference(int conferenceId);

public:
	// TODO: Deprecate onWebSocketConnection() and just rely on ConnectTransport() above.
	virtual void onWebSocketConnection(const HTTPRequest &request, WebSocket *ws);
	virtual void onOpen(WebSocket *ws);
	virtual void onClose(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws, WebSocket::MessageType type, const DWORD length);
	virtual void onMessageData(WebSocket *ws, const BYTE* data, const DWORD size);
	virtual void onWriteBufferEmpty(WebSocket *ws);
	virtual void onMessageEnd(WebSocket *ws);

private:
	typedef std::map<int, BFCPFloorControlServer *> Conferences;
	Conferences conferences;

public:
	struct ConnectionData
	{
		int conferenceId;
		int userId;
		bool closedByServer;
		UTF8Parser utf8;

		ConnectionData(int conferenceId, int userId)
		{
			this->userId = userId;
			this->conferenceId = conferenceId;
			this->closedByServer = false;
		}
	};
};


#endif
