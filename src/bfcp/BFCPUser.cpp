#include "bfcp.h"
#include "bfcp/BFCPUser.h"
#include "log.h"
#include <algorithm>  // std::find()


/* Instance members. */


BFCPUser::BFCPUser(int userId, int conferenceId) :
	userId(userId),
	conferenceId(conferenceId),
	isChair(false),
	transport(NULL)
{
	//Create mutex
	pthread_mutex_init(&mutex,NULL);
}

BFCPUser::~BFCPUser()
{
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

int BFCPUser::GetUserId()
{
	return userId;
}


void BFCPUser::SetChair()
{
	isChair = true;
}


void BFCPUser::UnsetChair()
{
	isChair = false;
}


bool BFCPUser::IsChair()
{
	return isChair;
}


void BFCPUser::SetTransport(WebSocket *transport)
{
	// Lock mutex.
	pthread_mutex_lock(&mutex);
	// Set the new transport for this user.
	this->transport = transport;
	// Un Lock mutex.
	pthread_mutex_unlock(&mutex);
}


// This is called by the BFCPFloorControlServer when the user connection is remotely closed
void BFCPUser::UnsetTransport()
{
	// Lock mutex.
	pthread_mutex_lock(&mutex);
	//Disconnected
	transport = NULL;
	// Un Lock mutex.
	pthread_mutex_unlock(&mutex);
}


// Called by the BFCPFloorControlServer when a user is removed ot the conference is terminated.
void BFCPUser::CloseTransport(const WORD code, const std::wstring& reason)
{
	::Debug("BFCPUser::CloseTransport() | start\n");

	// Lock mutex.
	pthread_mutex_lock(&mutex);

	if (transport) {
		BFCP::ConnectionData *conn_data = (BFCP::ConnectionData *)transport->GetUserData();
		// Important: set this flag to true so revoking process does not take twice.
		conn_data->closedByServer = true;
		//Close transport
		transport->Close(code, reason);
		transport = NULL;
	}

	// Un Lock mutex.
	pthread_mutex_unlock(&mutex);

	::Debug("BFCPUser::CloseTransport() | end\n");
}


bool BFCPUser::IsConnected()
{
	return (transport ? true : false);
}


void BFCPUser::SendMessage(BFCPMessage *msg)
{
	::Debug("BFCPUser::SendMessage() | start\n");

	// Lock mutex.
	pthread_mutex_lock(&mutex);

	if (transport) {
		// TODO: Here we assume that the user is connected via WS. transport should be a BFCPTransport
		// instance with inherited classes BFCPWebSocketTransport and BFCPBinaryTransport with
		// different implementations of the SendMessage() method.
		transport->SendMessage(msg->Stringify());
	} else {
		::Debug("BFCPUser::SendMessage() | user '%d' not connected, cannot send message\n", userId);
	}

	// Un Lock mutex.
	pthread_mutex_unlock(&mutex);

	::Debug("BFCPUser::SendMessage() | end\n");
}


void BFCPUser::ResetQueriedFloorIds()
{
	queriedFloorIds.clear();
}


void BFCPUser::AddQueriedFloorId(int floorId)
{
	// Avoid duplicated floorIds.
	if (std::find(queriedFloorIds.begin(), queriedFloorIds.end(), floorId) != queriedFloorIds.end())
		return;

	queriedFloorIds.push_back(floorId);
}


int BFCPUser::CountQueriedFloorIds()
{
	return queriedFloorIds.size();
}


bool BFCPUser::HasQueriedFloorId(int floorId)
{
	if (std::find(queriedFloorIds.begin(), queriedFloorIds.end(), floorId) != queriedFloorIds.end())
		return true;
	else
		return false;
}


int BFCPUser::GetQueriedFloorId(unsigned int index)
{
	if (index >= queriedFloorIds.size())
		throw BFCPMessage::AttributeNotFound("'queriedFloorId' not in range");

	return queriedFloorIds[index];
}


void BFCPUser::Dump()
{
	::Debug("[BFCPUser]\n");
	::Debug("- userId: %d\n", userId);
	::Debug("- conferenceId: %d\n", conferenceId);
	if (isChair) {
		::Debug("- chair: yes\n");
	}
	else {
		::Debug("- chair: no\n");
	}
	if (transport) {
		::Debug("- connected\n");
	}
	else {
		::Debug("- not connected\n");
	}
	int num_floors = queriedFloorIds.size();
	for (int i=0; i<num_floors; i++) {
		::Debug("- queriedFloorId: %d\n", queriedFloorIds[i]);
	}
	::Debug("[/BFCPUser]\n");
}
