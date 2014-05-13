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
}


int BFCPUser::GetUserId()
{
	return this->userId;
}


void BFCPUser::SetChair()
{
	this->isChair = true;
}


void BFCPUser::UnsetChair()
{
	this->isChair = false;
}


bool BFCPUser::IsChair()
{
	return this->isChair;
}


void BFCPUser::SetTransport(WebSocket *transport)
{
	// Set the new transport for this user.
	this->transport = transport;
}


// This is called by the BFCPFloorControlServer when a user connection is disconnected from
// the client, so we don't need to close it.
void BFCPUser::UnsetTransport()
{
	this->transport = NULL;
}


// Called by the BFCPFloorControlServer when a user is removed ot the conference is terminated.
void BFCPUser::CloseTransport(const WORD code, const std::wstring& reason)
{
	::Debug("BFCPUser::CloseTransport() | start\n");

	if (this->transport) {
		BFCP::ConnectionData *conn_data = (BFCP::ConnectionData *)this->transport->GetUserData();
		// Important: set this flag to true so revoking process does not take twice.
		conn_data->closedByServer = true;

		this->transport->Close(code, reason);
		this->transport = NULL;
	}

	::Debug("BFCPUser::CloseTransport() | end\n");
}


bool BFCPUser::IsConnected()
{
	return (this->transport ? true : false);
}


void BFCPUser::SendMessage(BFCPMessage *msg)
{
	::Debug("BFCPUser::SendMessage() | start\n");

	if (! this->transport) {
		::Debug("BFCPUser::SendMessage() | user '%d' not connected, cannot send message\n", this->userId);
		return;
	}

	// TODO: Here we assume that the user is connected via WS. transport should be a BFCPTransport
	// instance with inherited classes BFCPWebSocketTransport and BFCPBinaryTransport with
	// different implementations of the SendMessage() method.
	this->transport->SendMessage(msg->Stringify());

	::Debug("BFCPUser::SendMessage() | end\n");
}


void BFCPUser::ResetQueriedFloorIds()
{
	this->queriedFloorIds.clear();
}


void BFCPUser::AddQueriedFloorId(int floorId)
{
	// Avoid duplicated floorIds.
	if (std::find(this->queriedFloorIds.begin(), this->queriedFloorIds.end(), floorId) != this->queriedFloorIds.end())
		return;

	this->queriedFloorIds.push_back(floorId);
}


int BFCPUser::CountQueriedFloorIds()
{
	return this->queriedFloorIds.size();
}


bool BFCPUser::HasQueriedFloorId(int floorId)
{
	if (std::find(this->queriedFloorIds.begin(), this->queriedFloorIds.end(), floorId) != this->queriedFloorIds.end())
		return true;
	else
		return false;
}


int BFCPUser::GetQueriedFloorId(unsigned int index)
{
	if (index >= this->queriedFloorIds.size())
		throw BFCPMessage::AttributeNotFound("'queriedFloorId' not in range");

	return this->queriedFloorIds[index];
}


void BFCPUser::Dump()
{
	::Debug("[BFCPUser]\n");
	::Debug("- userId: %d\n", this->userId);
	::Debug("- conferenceId: %d\n", this->conferenceId);
	if (this->isChair) {
		::Debug("- chair: yes\n");
	}
	else {
		::Debug("- chair: no\n");
	}
	if (this->transport) {
		::Debug("- connected\n");
	}
	else {
		::Debug("- not connected\n");
	}
	int num_floors = this->queriedFloorIds.size();
	for (int i=0; i<num_floors; i++) {
		::Debug("- queriedFloorId: %d\n", this->queriedFloorIds[i]);
	}
	::Debug("[/BFCPUser]\n");
}
