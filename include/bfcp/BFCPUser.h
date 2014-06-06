#ifndef BFCPUSER_H
#define BFCPUSER_H


#include "bfcp/BFCPMessage.h"
#include "websocketconnection.h"
#include <pthread.h>
#include <vector>
#include <string>


class BFCPUser
{
public:
	BFCPUser(int userId, int conferenceId);
	~BFCPUser();

	int GetUserId();
	void SetChair();
	void UnsetChair();
	bool IsChair();
	void SetTransport(WebSocket *transport);
	void UnsetTransport();
	void CloseTransport(const WORD code, const std::wstring& reason);
	bool IsConnected();
	void SendMessage(BFCPMessage *msg);
	void ResetQueriedFloorIds();
	void AddQueriedFloorId(int floorId);
	int CountQueriedFloorIds();
	bool HasQueriedFloorId(int floorId);
	int GetQueriedFloorId(unsigned int index);
	void Dump();

private:
	int userId;
	int conferenceId;
	bool isChair;
	// The transport of the user.
	// TODO: Must be a generic BFCPTransport class for both TCP/WS.
	WebSocket *transport;
	// The list of floors the user has subscribed to via FloorQuery request.
	// Note that just the floors within the last received FloorQuery are considered.
	std::vector<int> queriedFloorIds;
	// Mutex for blocking access to the transport.
	pthread_mutex_t mutex;
};


#endif
