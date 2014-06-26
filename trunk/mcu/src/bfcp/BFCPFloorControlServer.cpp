#include "bfcp/BFCPFloorControlServer.h"
#include "bfcp/messages.h"
#include "bfcp/attributes.h"
#include "log.h"
#include "use.h"


BFCPFloorControlServer::BFCPFloorControlServer(int conferenceId, BFCPFloorControlServer::Listener *listener) :
	conferenceId(conferenceId),
	listener(listener),
	floorRequestCounter(1),
	ending(false)
{
}


BFCPFloorControlServer::~BFCPFloorControlServer()
{
	::Debug("BFCPFloorControlServer::~BFCPFloorControlServer()\n");

	//Lock
	users.WaitUnusedAndLock();
	// Clear Users, Floors and FloorRequests.
	for (BFCPFloorControlServer::Users::iterator it = this->users.begin() ; it != this->users.end(); ++it) {
		delete it->second;
	}
	this->users.clear();
	//Unlock
	users.Unlock();

	for (BFCPFloorControlServer::RemovedUsers::iterator it = this->removedUsers.begin() ; it != this->removedUsers.end(); ++it) {
		delete *it;
	}
	this->removedUsers.clear();

	this->floors.clear();

	for (BFCPFloorControlServer::FloorRequests::iterator it = this->floorRequests.begin() ; it != this->floorRequests.end(); ++it) {
		delete it->second;
	}
	this->floorRequests.clear();
}


int BFCPFloorControlServer::GetConferenceId()
{
	return this->conferenceId;
}


bool BFCPFloorControlServer::AddUser(int userId)
{
	if (this->ending)
		return ::Error("BFCPFloorControlServer::AddUser() already ended\n");

	if (userId < 1) {
		::Error("BFCPFloorControlServer::AddUser() | userId must be > 0\n");
		return false;
	}

	//Lock users for writting
	users.WaitUnusedAndLock();

	//Find user
	Users::iterator it = users.find(userId);

	// If the userId already exists then abort.
	if (it != users.end())
	{
		//Unlock
		users.Unlock();
		//Error
		return ::Error("BFCPFloorControlServer::AddUser() | userId '%d' already exists in conference '%d'\n", userId, this->conferenceId);
	}

	// Add to the Users map.
	this->users[userId] = new BFCPUser(userId, this->conferenceId);

	//Unlock
	users.Unlock();

	::Log("BFCPFloorControlServer::AddUser() | user '%d' added to conference '%d'\n", userId, this->conferenceId);
	return true;
}


bool BFCPFloorControlServer::RemoveUser(int userId)
{
	if (this->ending)
		return ::Error("BFCPFloorControlServer::RemoveUser() already ended\n");

	//Lock users for writting
	users.WaitUnusedAndLock();

	//Find user
	Users::iterator it = users.find(userId);

	// If the userId did not exist then abort.
	if (it == users.end())
	{
		//Unlock
		users.Unlock();
		//Error
		return ::Error("BFCPFloorControlServer::RemoveUser() | userId '%d' does not exist in conference '%d'\n", userId, this->conferenceId);
	}

	//Get user
	BFCPUser* user = it->second;

	//Delete from map
	users.erase(it);

	//Unlock
	users.Unlock();

	// Revoke user's ongoing FloorRequests.
	::Debug("BFCPFloorControlServer::RemoveUser() | calling RevokeUserFloorRequests(%d)\n", userId);
	RevokeUserFloorRequests(user);
	::Debug("BFCPFloorControlServer::RemoveUser() | RevokeUserFloorRequests(%d) returns\n", userId);

	// Close its transport (if connected).
	// TODO: log?
	user->CloseTransport(4001, L"you have been removed from the BFCP conference");

	// Don't delete the user (as other thread may have retrieved it before). Instead move it to removedUsers.
	removedUsers.push_back(user);

	::Log("BFCPFloorControlServer::RemoveUser() | user '%d' removed from conference '%d'\n", userId, this->conferenceId);
	return true;
}


bool BFCPFloorControlServer::SetChair(int userId)
{
	if (this->ending) { return false; }

	BFCPUser* user = GetUser(userId);

	if (! user) {
		::Error("BFCPFloorControlServer::SetChair() | user '%d' does not exist\n", userId);
		return false;
	}

	//As we are setting also the chair, lock for writting
	users.WaitUnusedAndLock();

	// Unset the current chair.
	for (BFCPFloorControlServer::Users::iterator it=this->users.begin(); it!=this->users.end(); ++it) {
		BFCPUser* otherUser = it->second;
		otherUser->UnsetChair();
	}

	::Log("BFCPFloorControlServer::SetChair() | user '%d' becomes chair\n", userId);
	user->SetChair();

	//Unlock after chair is set
	users.Unlock();

	return true;
}


bool BFCPFloorControlServer::AddFloor(int floorId)
{
	if (this->ending) { return false; }

	if (floorId < 1) {
		::Error("BFCPFloorControlServer::AddFloor() | floorId must be > 0\n");
		return false;
	}

	// Ensure the floor does not already exist.
	if (this->HasFloor(floorId)) {
		::Error("BFCPFloorControlServer::AddFloor() | floor '%d' already exists\n", floorId);
		return false;
	}

	// Add to the Floors set.
	this->floors.insert(floorId);

	::Log("BFCPFloorControlServer::AddFloor() | floor '%d' added to conference '%d'\n", floorId, this->conferenceId);
	return true;
}


bool BFCPFloorControlServer::GrantFloorRequest(int floorRequestId)
{
	if (this->ending) { return false; }

	::Debug("BFCPFloorControlServer::GrantFloorRequest() | start | [floorRequestId: %d]\n", floorRequestId);

	BFCPFloorRequest *floorRequest = GetFloorRequest(floorRequestId);
	if (! floorRequest) {
		::Error("BFCPFloorControlServer::GrantFloorRequest() | FloorRequest '%d' does not exist\n", floorRequestId);
		return false;
	}


	/* Check that the FloorRequest is in a proper status. */

	if (! floorRequest->CanBeGranted()) {
		::Error("BFCPFloorControlServer::GrantFloorRequest() | cannot grant FloorRequest '%d' which is in status '%ls'\n", floorRequestId, floorRequest->GetStatusString().c_str());
		return false;
	}


	/* Revoke any other(s) FloorRequests owning any floor in this FloorRequest. */

	// Get all the ongoing FloorRequests owning any floor in this FloorRequest.
	std::vector<BFCPFloorRequest*> floorRequestsToRevoke = GetFloorRequestsForFloors(floorRequest->GetFloorIds());

	// Revoke the Granted ones (the current FloorRequest won't be revoked since it is not granted yet).
	for (int i=0; i<floorRequestsToRevoke.size(); i++) {
		::Debug("BFCPFloorControlServer::GrantFloorRequest() | revoking FloorRequest %d/%d\n", i, floorRequestsToRevoke.size());
		BFCPFloorRequest* floorRequestToRevoke = floorRequestsToRevoke[i];

		if (! floorRequestToRevoke->IsGranted())
			continue;
		RevokeFloorRequest(floorRequestToRevoke->GetFloorRequestId(), L"granted to other user");
	}


	/* Update the current FloorRequest to "Granted" and send the FloorRequestStatus notification
	 * to the FloorRequest sender. */

	::Debug("BFCPFloorControlServer::GrantFloorRequest() | FloorRequest '%d' before beeing granted:\n", floorRequestId);
	floorRequest->Dump();

	// Update the FloorRequest status to "Granted".
	::Log("BFCPFloorControlServer::GrantFloorRequest() | FloorRequest '%d' has been granted\n", floorRequestId);
	floorRequest->SetStatus(BFCPAttrRequestStatus::Granted);

	// Notify to the FloorRequest sender with a "Granted" FloorRequestStatus notification.
	::Debug("BFCPFloorControlServer::GrantFloorRequest() | sending 'Granted' FloorRequestStatus notification for FloorRequest '%d'\n", floorRequestId);
	BFCPMsgFloorRequestStatus *floorRequestStatus = floorRequest->CreateFloorRequestStatus();

	// Send the FloorRequestStatus notification to the requester of the FloorRequest.
	SendMessage(floorRequestStatus);

	// Delete it.
	delete floorRequestStatus;


	/* Notify with FloorStatus notifications to users who queried the status of any of the floors
	 * in the current FloorRequest. */

	NotifyForFloorRequest(floorRequest);


	/* Notify the chair */

	::Debug("BFCPFloorControlServer::GrantFloorRequest() | calling listener->onFloorGranted(%d)\n", floorRequestId);
	this->listener->onFloorGranted(floorRequestId, floorRequest->GetBeneficiaryId(), floorRequest->GetFloorIds());
	::Debug("BFCPFloorControlServer::GrantFloorRequest() | listener->onFloorGranted(%d) returns\n", floorRequestId);

	::Debug("BFCPFloorControlServer::GrantFloorRequest() | end | [floorRequestId: %d]\n", floorRequestId);

	return true;
}


bool BFCPFloorControlServer::DenyFloorRequest(int floorRequestId, std::wstring statusInfo)
{
	::Debug("BFCPFloorControlServer::DenyFloorRequest() | start | [floorRequestId: %d]\n", floorRequestId);

	BFCPFloorRequest *floorRequest = GetFloorRequest(floorRequestId);
	if (! floorRequest) {
		::Error("BFCPFloorControlServer::DenyFloorRequest() | FloorRequest '%d' does not exist\n", floorRequestId);
		return false;
	}


	/* Check that the FloorRequest is in a proper status. */

	if (! floorRequest->CanBeDenied()) {
		::Error("BFCPFloorControlServer::DenyFloorRequest() | cannot deny FloorRequest '%d' which is in status '%ls'\n", floorRequestId, floorRequest->GetStatusString().c_str());
		return false;
	}


	/* Update the current FloorRequest to "Denied" and send the FloorRequestStatus notification
	 * to the FloorRequest sender. */

	::Debug("BFCPFloorControlServer::DenyFloorRequest() | FloorRequest '%d' before beeing denied:\n", floorRequestId);
	floorRequest->Dump();

	// Update the FloorRequest status to "Denied".
	::Log("BFCPFloorControlServer::DenyFloorRequest() | FloorRequest '%d' has been denied\n", floorRequestId);
	floorRequest->SetStatus(BFCPAttrRequestStatus::Denied);

	// Notify to the FloorRequest sender with a "Denied" FloorRequestStatus notification.
	::Debug("BFCPFloorControlServer::DenyFloorRequest() | sending 'Denied' FloorRequestStatus notification for FloorRequest '%d'\n", floorRequestId);
	BFCPMsgFloorRequestStatus *floorRequestStatus = floorRequest->CreateFloorRequestStatus();
	if (! statusInfo.empty())
		floorRequestStatus->SetDescription(statusInfo);

	// Send the FloorRequestStatus notification to the requester of the FloorRequest.
	SendMessage(floorRequestStatus);

	// Delete it.
	delete floorRequestStatus;


	/* Notify with FloorStatus notifications to users who queried the status of any of the floors
	 * in the current FloorRequest. */

	NotifyForFloorRequest(floorRequest);


	/* Delete the FloorRequest. */

	this->floorRequests.erase(floorRequestId);
	delete floorRequest;

	::Debug("BFCPFloorControlServer::DenyFloorRequest() | end | [floorRequestId: %d]\n", floorRequestId);

	return true;
}


bool BFCPFloorControlServer::RevokeFloorRequest(int floorRequestId, std::wstring statusInfo)
{
	::Debug("BFCPFloorControlServer::RevokeFloorRequest() | start | [floorRequestId: %d]\n", floorRequestId);

	BFCPFloorRequest *floorRequest = GetFloorRequest(floorRequestId);
	if (! floorRequest) {
		::Error("BFCPFloorControlServer::RevokeFloorRequest() | FloorRequest '%d' does not exist\n", floorRequestId);
		return false;
	}


	/* Check that the FloorRequest is in Granted status. */

	if (! floorRequest->IsGranted()) {
		::Error("BFCPFloorControlServer::RevokeFloorRequest() | cannot deny FloorRequest '%d' which is in status 'Granted'\n", floorRequestId);
		return false;
	}


	/* Update the current FloorRequest to "Revoked" and send the FloorRequestStatus notification
	 * to the FloorRequest sender. */

	::Debug("BFCPFloorControlServer::RevokeFloorRequest() | FloorRequest '%d' before beeing revoked:\n", floorRequestId);
	floorRequest->Dump();

	// Update the FloorRequest status to "Revoked".
	::Log("BFCPFloorControlServer::RevokeFloorRequest() | FloorRequest '%d' has been revoked\n", floorRequestId);
	floorRequest->SetStatus(BFCPAttrRequestStatus::Revoked);

	// Notify to the FloorRequest sender with a "Revoked" FloorRequestStatus notification.
	::Debug("BFCPFloorControlServer::RevokeFloorRequest() | sending 'Revoked' FloorRequestStatus notification for FloorRequest '%d'\n", floorRequestId);
	BFCPMsgFloorRequestStatus *floorRequestStatus = floorRequest->CreateFloorRequestStatus();
	if (! statusInfo.empty())
		floorRequestStatus->SetDescription(statusInfo);

	// Send the FloorRequestStatus notification to the requester of the FloorRequest.
	SendMessage(floorRequestStatus);

	// Delete it.
	delete floorRequestStatus;


	/* Notify with FloorStatus notifications to users who queried the status of any of the floors
	 * in the current FloorRequest. */

	NotifyForFloorRequest(floorRequest);


	/* Notify the chair. */

	// But first remove the FloorRequest from the map so in the ugly case in which the
	// chair calls to RevokeFloor() for this same FloorRequest it will fail.
	this->floorRequests.erase(floorRequestId);

	if (! this->ending) {
		::Debug("BFCPFloorControlServer::RevokeFloorRequest() | calling listener->onFloorReleased(%d)\n", floorRequestId);
		this->listener->onFloorReleased(floorRequestId, floorRequest->GetBeneficiaryId(), floorRequest->GetFloorIds());
		::Debug("BFCPFloorControlServer::RevokeFloorRequest() | listener->onFloorReleased(%d) returns\n", floorRequestId);
	}


	/* Delete the FloorRequest. */

	// NOTE: the FloorRequest may has been removed by the chair when notified onFloorReleased, so check it!
	if (GetFloorRequest(floorRequestId)) {
		delete floorRequest;
	}

	::Debug("BFCPFloorControlServer::RevokeFloorRequest() | end | [floorRequestId: %d]\n", floorRequestId);

	return true;
}


int BFCPFloorControlServer::GetGrantedFloorRequestId(int floorId)
{
	if (this->ending) { return 0; }

	if (! HasFloor(floorId)) {
		::Error("BFCPFloorControlServer::GetGrantedFloorRequestId() | floor '%d' does not exist\n", floorId);
		return 0;
	}

	// Get all the ongoing FloorRequest including the given floor.
	std::vector<BFCPFloorRequest*> floorRequests = GetFloorRequestsForFloor(floorId);

	// Choose the first one that is Granted (can only be one).
	int num_floor_requests = floorRequests.size();
	for (int i=0; i<num_floor_requests; i++) {
		if (floorRequests[i]->IsGranted()) {
			return floorRequests[i]->GetFloorRequestId();
		}
	}

	return 0;
}


void BFCPFloorControlServer::End()
{
	if (this->ending) { return; }

	::Log("BFCPFloorControlServer::End() | terminanting conference '%d'\n", this->conferenceId);

	this->ending = true;

	// Revoke all the ongoing FloorRequests.
	::Log("BFCPFloorControlServer::End() | revoking/dening ongoing FloorRequests in conference '%d'\n", this->conferenceId);

	BFCPFloorControlServer::FloorRequests::iterator it;
	while (this->floorRequests.size() > 0) {
		::Debug("BFCPFloorControlServer::End() | [number of ongoing FloorRequests: %d]\n", this->floorRequests.size());

		it = this->floorRequests.begin();
		BFCPFloorRequest* floorRequest = it->second;

		if (floorRequest->IsGranted()) {
			RevokeFloorRequest(floorRequest->GetFloorRequestId(), L"conference ended");
		}
		else {
			DenyFloorRequest(floorRequest->GetFloorRequestId(), L"conference ended");
		}
	}

	// Disconect all the users.
	::Log("BFCPFloorControlServer::End() | disconnecting users in conference '%d'\n", this->conferenceId);

	//Lock users from writing
	users.WaitUnusedAndLock();

	//Close all user's transports and erase map
	Users::iterator it2 = users.begin();

	//Until the end
	while( it2!=users.end())
	{
		::Debug("BFCPFloorControlServer::End() | [number of users: %d]\n", this->users.size());

		//Get users
		BFCPUser* user = it2->second;

		//erase from map and move forward iterator
		users.erase(it2++);

		// Close its transport (if connected).
		user->CloseTransport(4000, L"the BFCP conference has ended");

		// Delete user.
		delete user;
	}

	//Unlock map
	users.Unlock();
}


/**
 * Called by the transport layer upon a new WS connection.
 *
 * @param  userId          The userId of the connected user.
 * @param  ws              The WebSocket connection.
 * @return                 true when a valid user.
 */
bool BFCPFloorControlServer::UserConnected(int userId, WebSocket *ws)
{
	if (this->ending) { return false; }

	// The userId must exist in the list of users for this conference.
	BFCPUser *user = this->GetUser(userId);
	if (! user) {
		::Error("BFCPFloorControlServer::UserConnected() | connection from invalid userId '%d' ignored in conference '%d'\n", userId, this->conferenceId);
		return false;
	}

	// Valid user. Set (or replace) its transport.
	// If it was connected then reset first.
	if (user->IsConnected()) {
		::Log("BFCPFloorControlServer::UserConnected() | user '%d' was already connected in conference '%d', reseting user\n", userId, this->conferenceId);

		// Reset queried floors.
		user->ResetQueriedFloorIds();

		// Revoke user's ongoing FloorRequests.
		::Debug("BFCPFloorControlServer::UserConnected() | calling RevokeUserFloorRequests(%d)\n", userId);
		RevokeUserFloorRequests(user);
		::Debug("BFCPFloorControlServer::UserConnected() | RevokeUserFloorRequests(%d) returns\n", userId);

		// Close the previous connection from this user.
		user->CloseTransport(4002, L"your user has connected from somewhere else");
	}
	user->SetTransport(ws);

	::Log("BFCPFloorControlServer::UserConnected() | user '%d' connected to conference '%d':\n", userId, this->conferenceId);
	user->Dump();

	return true;
}


/**
 * Called by the transport upon disconnection of a valid WS connection.
 *
 * @param  userId The userId associated (previously validated) to the connection.
 */
void BFCPFloorControlServer::UserDisconnected(int userId, bool closedByServer)
{
	BFCPUser *user = this->GetUser(userId);

	// The user does not longer exist it is has been removed via RemoveUser().
	if (! user) {
		return;
	}

	// If the server has disconnected the user then do nothing (already done).
	if (closedByServer) {
		::Log("BFCPFloorControlServer::UserDisconnected() | user '%d' disconnected by the server from conference '%d'\n", userId, this->conferenceId);
		return;
	}

	::Log("BFCPFloorControlServer::UserDisconnected() | user '%d' disconnected from conference '%d'\n", userId, this->conferenceId);

	// Remove the transport of the user (so we detect it as disconnected).
	user->UnsetTransport();

	// Reset queried floors.
	user->ResetQueriedFloorIds();

	// Don't do more if the conference is ending.
	if (this->ending) { return; }

	// Revoke user's ongoing FloorRequests.
	::Debug("BFCPFloorControlServer::UserDisconnected() | calling RevokeUserFloorRequests(%d)\n", userId);
	RevokeUserFloorRequests(user);
	::Debug("BFCPFloorControlServer::UserDisconnected() | RevokeUserFloorRequests(%d) returns\n", userId);
}


/**
 * Called by the transport when an invalid message has been received.
 */
void BFCPFloorControlServer::DisconnectUser(int userId, const WORD code, const std::wstring& reason)
{
	if (this->ending) { return; }

	::Debug("BFCPFloorControlServer::DisconnectUser() | start | [userId: %d]\n", userId);

	BFCPUser *user = this->GetUser(userId);

	// This should never happen.
	if (! user) {
		::Error("BFCPFloorControlServer::DisconnectUser() | user '%d' does not exist in the conference\n", userId);
		return;
	}

	::Log("BFCPFloorControlServer::UserDisconnected() | disconnecting user '%d' from conference '%d':\n", userId, this->conferenceId);

	// Reset queried floors.
	user->ResetQueriedFloorIds();

	// Revoke user's ongoing FloorRequests.
	::Debug("BFCPFloorControlServer::DisconnectUser() | calling RevokeUserFloorRequests(%d)\n", userId);
	RevokeUserFloorRequests(user);
	::Debug("BFCPFloorControlServer::DisconnectUser() | RevokeUserFloorRequests(%d) returns\n", userId);

	// Disconnect the transport.
	user->CloseTransport(code, reason);

	::Debug("BFCPFloorControlServer::DisconnectUser() | end | [userId: %d]\n", userId);
}


/**
 * BFCPFloorControlServer::MessageReceived Called by the transport upon receipt of a BFCP message.
 * @param msg The BFCP message.
 */
void BFCPFloorControlServer::MessageReceived(BFCPMessage *msg)
{
	if (this->ending) { return; }

	::Debug("BFCPFloorControlServer::MessageReceived() | start | %ls message received:\n", BFCPMessage::mapPrimitive2JsonStr[msg->GetPrimitive()].c_str());
	msg->Dump();

	int userId = msg->GetUserId();
	BFCPUser* user = GetUser(userId);

	if (! user) {
		::Error("BFCPFloorControlServer::MessageReceived() | userId '%d' does not exist, replying BFCP Error 'UserDoesNotExist'\n");
		ReplyError(msg, BFCPAttrErrorCode::UserDoesNotExist, L"user does not exist");
		return;
	}

	switch (msg->GetPrimitive())
	{
		case BFCPMessage::FloorRequest:
			{
				BFCPMsgFloorRequest *req = (BFCPMsgFloorRequest *)msg;
				ProcessFloorRequest(req, user);
			}
			break;

		case BFCPMessage::FloorRelease:
			{
				BFCPMsgFloorRelease *req = (BFCPMsgFloorRelease *)msg;
				ProcessFloorRelease(req, user);
			}
			break;

		case BFCPMessage::FloorQuery:
			{
				BFCPMsgFloorQuery *req = (BFCPMsgFloorQuery *)msg;
				ProcessFloorQuery(req, user);
			}
			break;

		case BFCPMessage::Hello:
			{
				BFCPMsgHello *req = (BFCPMsgHello *)msg;
				ProcessHello(req, user);
			}
			break;

		default:
			{
				// Reply UnknownPrimitive Error.
				::Error("BFCPFloorControlServer::MessageReceived() | replying BFCP Error 'UnknownPrimitive'\n");
				ReplyError(msg, BFCPAttrErrorCode::UnknownPrimitive, L"unknown or unsupported primitive");
			}
	}

	::Debug("BFCPFloorControlServer::MessageReceived() | end | %ls message processed:\n", BFCPMessage::mapPrimitive2JsonStr[msg->GetPrimitive()].c_str());
}


bool BFCPFloorControlServer::HasUser(int userId)
{
	return GetUser(userId)!=NULL;
}


BFCPUser* BFCPFloorControlServer::GetUser(int userId)
{
	//NO User yet
	BFCPUser* user = NULL;

	//Lock users
	users.IncUse();

	//Find user
	Users::iterator it = users.find(userId);

	//If found
	if (it != users.end())
		//Get it
		user = it->second;

	//Unlock
	users.DecUse();

	//Return user
	return user;
}


BFCPFloorRequest* BFCPFloorControlServer::GetFloorRequest(int floorRequestId)
{
	BFCPFloorControlServer::FloorRequests::iterator it = this->floorRequests.find(floorRequestId);
	if (it != this->floorRequests.end())
		return it->second;
	else
		return NULL;
}


std::vector<BFCPFloorRequest*> BFCPFloorControlServer::GetFloorRequestsForFloor(int floorId)
{
	std::vector<BFCPFloorRequest*> floorRequests;

	for (BFCPFloorControlServer::FloorRequests::iterator it = this->floorRequests.begin() ; it != this->floorRequests.end(); ++it) {
		BFCPFloorRequest* floorRequest = it->second;
		if (floorRequest->HasFloorId(floorId)) {
			floorRequests.push_back(floorRequest);
		}
	}

	return floorRequests;
}


std::vector<BFCPFloorRequest*> BFCPFloorControlServer::GetFloorRequestsForFloors(std::set<int> floorIds)
{
	std::vector<BFCPFloorRequest*> floorRequests;

	for (BFCPFloorControlServer::FloorRequests::iterator it = this->floorRequests.begin() ; it != this->floorRequests.end(); ++it) {
		BFCPFloorRequest* floorRequest = it->second;

		// If the FloorRequest contains any of the given floors then collect it.
		std::set<int>::iterator it2;
		for(it2=floorIds.begin(); it2 != floorIds.end(); ++it2) {
			int floorId = *it2;
			if (floorRequest->HasFloorId(floorId)) {
				floorRequests.push_back(floorRequest);
				break;  // No need to inspect more floors.
			}
		}
	}

	return floorRequests;
}


void BFCPFloorControlServer::NotifyForFloorRequest(BFCPFloorRequest* floorRequest)
{
	::Debug("BFCPFloorControlServer::NotifyForFloorRequest() | start | notifying changes in FloorRequest '%d' to subscribers\n", floorRequest->GetFloorRequestId());

	// Map of users and their queried affected floors due to the status change in
	// the given FloorRequest.
	std::map<BFCPUser*, std::set<int> > mapUsersAffectedFloorIds;
	std::set<int> affectedFloorIds = floorRequest->GetFloorIds();

	//Lock user map
	users.IncUse();

	// Fill the map.
	for (BFCPFloorControlServer::Users::iterator it=this->users.begin(); it!=this->users.end(); ++it) {
		BFCPUser* user = it->second;
		std::set<int> userAffectedfloorIds;

		for(std::set<int>::iterator it2=affectedFloorIds.begin(); it2 != affectedFloorIds.end(); ++it2) {
			int floorId = *it2;
			if (user->HasQueriedFloorId(floorId)) {
				userAffectedfloorIds.insert(floorId);
			}
		}

		// If at least one floor has been collected then collect the user.
		if (! userAffectedfloorIds.empty())
			mapUsersAffectedFloorIds[user] = userAffectedfloorIds;
	}
	//Unlock user map
	users.DecUse();

	// Create a single FloorStatus notification and modify it for each user and floor.
	BFCPMsgFloorStatus* floorStatus = new BFCPMsgFloorStatus(0, this->GetConferenceId(), 0);

	// Generate a FloorRequestInformation attribute for the given FloorRequest and append
	// it to the FloorStatus message.
	BFCPAttrFloorRequestInformation* floorRequestInformation = floorRequest->CreateFloorRequestInformation();
	floorStatus->AddFloorRequestInformation(floorRequestInformation);

	// For each user the FloorStatus must be sent as many times as affected floors the user
	// has (each FloorStatus with the corresponding floorId attribute).
	std::map<BFCPUser*, std::set<int> >::iterator it3;
	for (it3=mapUsersAffectedFloorIds.begin(); it3!=mapUsersAffectedFloorIds.end(); ++it3) {
		BFCPUser* user = it3->first;
		int userId = user->GetUserId();
		std::set<int> userAffectedfloorIds = it3->second;

		std::set<int>::iterator it4;
		for (it4=userAffectedfloorIds.begin(); it4!=userAffectedfloorIds.end(); ++it4)
		{
		    int floorId = *it4;

		    // Update the FloorStatus with the userId and floorId.
		    floorStatus->SetUserId(userId);
		    floorStatus->SetFloorId(floorId);

		    // Send the FloorStatus notification to the subscriber user.
		    SendMessage(floorStatus);
		}
	}

	// Delete the FloorStatus.
	delete floorStatus;

	::Debug("BFCPFloorControlServer::NotifyForFloorRequest() | end | notified changes in FloorRequest '%d' to subscribers\n", floorRequest->GetFloorRequestId());

}


void BFCPFloorControlServer::RevokeUserFloorRequests(BFCPUser* user)
{
	::Debug("BFCPFloorControlServer::RevokeUserFloorRequests() | start | revoking (or cancelling) ongoing FloorRequest of user '%d'\n", user->GetUserId());

	int userId = user->GetUserId();
	std::vector<int> userFloorRequests;

	// Store all the ongoing FloorRequestsIds for this user in a vector.
	BFCPFloorControlServer::FloorRequests::iterator it;
	for(it=this->floorRequests.begin(); it!=this->floorRequests.end(); it++) {
		BFCPFloorRequest* floorRequest = it->second;

		// Just collect those FloorRequests whose beneficiary is the given user.
		if (floorRequest->GetBeneficiaryId() == userId)
			userFloorRequests.push_back(floorRequest->GetFloorRequestId());
	}

	// For each stored FloorRequestId check that it still exists (the chair may delete it
	// when notified).
	for(int i=0; i<userFloorRequests.size(); i++) {
		::Debug("BFCPFloorControlServer::RevokeUserFloorRequests() | revoking FloorRequest %d/%d\n", i, userFloorRequests.size());

		int floorRequestId = userFloorRequests[i];
		BFCPFloorRequest* floorRequest = GetFloorRequest(floorRequestId);

		// Has been removed!
		if (! floorRequest)
			continue;

		if (floorRequest->IsGranted()) {
			RevokeFloorRequest(floorRequest->GetFloorRequestId(), L"FloorRequests from this user have been revoked or denied");
		}
		else {
			DenyFloorRequest(floorRequest->GetFloorRequestId(), L"FloorRequests from this user have been revoked or denied");
		}
	}

	::Debug("BFCPFloorControlServer::RevokeUserFloorRequests() | end | revoke (or cancelled) ongoing FloorRequest of user '%d'\n", userId);
}


bool BFCPFloorControlServer::HasFloor(int floorId)
{
	return (this->floors.find(floorId) != this->floors.end()) ? true : false;
}


void BFCPFloorControlServer::ProcessFloorRequest(BFCPMsgFloorRequest *req, BFCPUser* user)
{
	// Check that floorId value(s) in the request exist.
	int num_floors = req->CountFloorIds();
	for (int i=0; i < num_floors; i++) {
		int floorId = req->GetFloorId(i);
		if (! HasFloor(floorId)) {
			::Error("BFCPFloorControlServer::ProcessFloorRequest() | requesting non existing floor '%d'\n", floorId);
			ReplyError(req, BFCPAttrErrorCode::InvalidFloorId, L"requested floor does not exist");
			return;
		}
	}

	// Check that the beneficiaryId (if present) is a valid user.
	if (req->HasBeneficiaryId()) {
		if (! HasUser(req->GetBeneficiaryId())) {
			::Error("BFCPFloorControlServer::ProcessFloorRequest() | beneficiary '%d' does not exist\n", req->GetBeneficiaryId());
			ReplyError(req, BFCPAttrErrorCode::UserDoesNotExist, L"beneficiary does not exist");
			return;
		}
	}

	// Set an unique floorRequestId.
	int floorRequestId = this->floorRequestCounter++;

	// Create a BFCPFloorRequest.
	BFCPFloorRequest* floorRequest = new BFCPFloorRequest(floorRequestId, req->GetUserId(), this->conferenceId);
	if (req->HasBeneficiaryId())
		floorRequest->SetBeneficiaryId(req->GetBeneficiaryId());
	for (int i=0; i < req->CountFloorIds(); i++)
		floorRequest->AddFloorId(req->GetFloorId(i));

	// Add to the floorRequests map.
	this->floorRequests[floorRequestId] = floorRequest;
	::Log("BFCPFloorControlServer::ProcessFloorRequest() | FloorRequest '%d' added to conference '%d'\n", floorRequestId, this->conferenceId);

	// Build a "Pending" FloorRequestStatus response.
	BFCPMsgFloorRequestStatus *floorRequestStatus = floorRequest->CreateFloorRequestStatus(req->GetTransactionId());

	// Send it.
	SendMessage(floorRequestStatus);

	// Delete it.
	delete floorRequestStatus;

	// If the FloorRequest comes from the chair automatically grant it.
	if (user->IsChair()) {
		::Log("BFCPFloorControlServer::ProcessFloorRequest() | the FloorRequest sender is a chair, request authorized\n");
		GrantFloorRequest(floorRequestId);
	}
	// Otherwise notify the chair.
	else {
		::Log("BFCPFloorControlServer::ProcessFloorRequest() | the FloorRequest sender is not a chair, let's ask the chair\n");
		::Debug("BFCPFloorControlServer::ProcessFloorRequest() | calling listener->onFloorRequest(%d)\n", floorRequestId);
		this->listener->onFloorRequest(floorRequestId, floorRequest->GetUserId(), floorRequest->GetBeneficiaryId(), floorRequest->GetFloorIds());
		::Debug("BFCPFloorControlServer::ProcessFloorRequest() | listener->onFloorRequest(%d) returns\n", floorRequestId);
	}
}


void BFCPFloorControlServer::ProcessFloorRelease(BFCPMsgFloorRelease *req, BFCPUser* user)
{
	int floorRequestId = req->GetFloorRequestId();
	BFCPFloorRequest* floorRequest = GetFloorRequest(floorRequestId);
	bool must_notify_chair = false;

	// Check that FloorRequest exists.
	if (! floorRequest) {
		::Error("BFCPFloorControlServer::ProcessFloorRelease() | FloorRequest '%d' does not exist\n", floorRequestId);
		ReplyError(req, BFCPAttrErrorCode::FloorRequestIdDoesNotExist, L"FloorRequest does not exist");
		return;
	}

	int floorReleaseSender = user->GetUserId();
	int floorRequestSender = floorRequest->GetUserId();
	int floorRequestBeneficiary = floorRequest->GetBeneficiaryId();

	// The FloorRelease is accepted in these cases:
	// - The FloorRelease sender matches the sender of the FloorRequest.
	// - The FloorRelease sender matches the beneficiary of the FloorRequest which is Granted.
	// - The FloorRelease sender is a chair and the FloorRequest is Granted.
	if (floorReleaseSender == floorRequestSender) {
		::Log("BFCPFloorControlServer::ProcessFloorRelease() | the FloorRelease sender matches the FloorRequest sender, release authorized\n");
	}
	else if (floorReleaseSender == floorRequestBeneficiary) {
		::Log("BFCPFloorControlServer::ProcessFloorRelease() | the FloorRelease sender matches the FloorRequest beneficiary, release authorized\n");
	}
	else if (user->IsChair()) {
		::Log("BFCPFloorControlServer::ProcessFloorRelease() | the FloorRelease sender is a chair, release authorized\n");
	}
	else {
		::Error("BFCPFloorControlServer::ProcessFloorRelease() | FloorRelease not allowed for this sender\n");
		ReplyError(req, BFCPAttrErrorCode::UnauthorizedOperation, L"you cannot release that FloorRequest");
		return;
	}

	// If it is not granted then cancel it.
	if (! floorRequest->IsGranted() && floorRequest->CanBeCancelled()) {
		// Update the status of the FloorRequest to "Cancelled".
		floorRequest->SetStatus(BFCPAttrRequestStatus::Cancelled);
	}

	// If it granted then release it (or revoke).
	else if (floorRequest->IsGranted()) {
		if (floorReleaseSender == floorRequestBeneficiary) {
			// Update the status of the FloorRequest to "Released".
			floorRequest->SetStatus(BFCPAttrRequestStatus::Released);
		}
		else {
			// Update the status of the FloorRequest to "Revoked".
			floorRequest->SetStatus(BFCPAttrRequestStatus::Revoked);
		}

		// Notify the chair (later, to avoid that the chair revokes the same FloorRequest and we
		// get a crash below).
		must_notify_chair = true;
	}

	// Otherwise cannot release it.
	else {
		::Error("BFCPFloorControlServer::ProcessFloorRelease() | cannot release a FloorRequest in status '%ls'\n", floorRequest->GetStatusString().c_str());
		ReplyError(req, BFCPAttrErrorCode::UnauthorizedOperation, L"cannot release or cancel the FloorRequest due to its current status");
		return;
	}


	// Create a FloorRequestStatus response for the FloorRelease sender.
	BFCPMsgFloorRequestStatus *floorRequestStatus = floorRequest->CreateFloorRequestStatus(req->GetTransactionId());
	floorRequestStatus->SetUserId(floorReleaseSender);

	// Send it.
	SendMessage(floorRequestStatus);

	// Delete it.
	delete floorRequestStatus;

	// If the FloorRelease sender does not match the FloorRequest sender then also send a
	// FloorRequestStatus notification to the FloorRequest sender.
	if (floorReleaseSender != floorRequestSender) {
		BFCPMsgFloorRequestStatus *floorRequestStatus = floorRequest->CreateFloorRequestStatus();
		floorRequestStatus->SetUserId(floorRequestSender);

		// Send it.
		SendMessage(floorRequestStatus);

		// Delete it.
		delete floorRequestStatus;
	}


	/* Notify with FloorStatus notifications to users who queried the status of any of the floors
	 * in the current FloorRequest. */

	NotifyForFloorRequest(floorRequest);


	/* Notify the chair. */

	// But first remove the FloorRequest from the map so in the ugly case in which the chair calls
	// to RevokeFloor() for this same FloorRequest it will fail.
	this->floorRequests.erase(floorRequestId);

	if (must_notify_chair) {
		::Debug("BFCPFloorControlServer::ProcessFloorRelease() | calling listener->onFloorReleased(%d)\n", floorRequestId);
		this->listener->onFloorReleased(floorRequestId, floorRequestBeneficiary, floorRequest->GetFloorIds());
		::Debug("BFCPFloorControlServer::ProcessFloorRelease() | listener->onFloorReleased(%d) returns\n", floorRequestId);
	}

	/* Delete the FloorRequest. */

	// NOTE: the FloorRequest may has been removed by the chair when notified onFloorReleased, so check it!
	if (GetFloorRequest(floorRequestId)) {
		delete floorRequest;
	}

	return;
}


void BFCPFloorControlServer::ProcessFloorQuery(BFCPMsgFloorQuery *req, BFCPUser* user)
{
	// Check that floorId value(s) in the request exist.
	int num_floors = req->CountFloorIds();
	for (int i=0; i < num_floors; i++) {
		int floorId = req->GetFloorId(i);
		if (! HasFloor(floorId)) {
			::Error("BFCPFloorControlServer::ProcessFloorQuery() | requesting non existing floor '%d'\n", floorId);
			ReplyError(req, BFCPAttrErrorCode::InvalidFloorId, L"requested floor does not exist");
			return;
		}
	}

	// Reset the list of floors this user is subscribed to.
	user->ResetQueriedFloorIds();

	// And insert the Floors included in the new FloorQuery.
	for (int i=0; i<num_floors; i++)
		user->AddQueriedFloorId(req->GetFloorId(i));

	::Debug("BFCPFloorControlServer::ProcessFloorQuery() | user '%d' information updated:\n", user->GetUserId());
	user->Dump();

	int num_queried_floors = user->CountQueriedFloorIds();

	// If the user has no queried floors then reply an empty FloorStatus and exit.
	if (num_queried_floors == 0) {
		::Debug("BFCPFloorControlServer::ProcessFloorQuery() | no queried floors, replying empty FloorStatus\n");
		BFCPMsgFloorStatus* floorStatus = new BFCPMsgFloorStatus(req->GetTransactionId(), this->GetConferenceId(), req->GetUserId());
		SendMessage(floorStatus);
		delete floorStatus;
		return;
	}

	// Otherwise, get the queried floors from the user (no duplicated floorIds there) and
	// generate a FloorStatus response for one of them and FloorStatus notifications for
	// the others.
	for (int i=0; i<num_queried_floors; i++) {
		BFCPMsgFloorStatus* floorStatus;
		int floorId = user->GetQueriedFloorId(i);

		// First FloorStatus will be a response to the FloorQuery.
		if (i == 0)
			floorStatus = new BFCPMsgFloorStatus(req->GetTransactionId(), this->GetConferenceId(), req->GetUserId());
		// Other FloorStatus are notifications.
		else
			floorStatus = new BFCPMsgFloorStatus(0, this->GetConferenceId(), req->GetUserId());

		// Set the floorId.
		floorStatus->SetFloorId(floorId);

		// Get all the ongoing FloorRequest that include this floor.
		std::vector<BFCPFloorRequest*> floorRequests = GetFloorRequestsForFloor(floorId);

		::Debug("BFCPFloorControlServer::ProcessFloorQuery() | found %d ongoing FloorRequests related to floor '%d'\n", floorRequests.size(), floorId);

		// Generate a FloorRequestInformation attribute from each FloorRequest and append
		// it to the FloorStatus message.
		for (int j=0; j<floorRequests.size(); j++) {
			BFCPAttrFloorRequestInformation* floorRequestInformation = floorRequests[j]->CreateFloorRequestInformation();
			floorStatus->AddFloorRequestInformation(floorRequestInformation);
		}

		// Send the response o notification to the user who sent the FloorQuery.
		SendMessage(floorStatus);

		delete floorStatus;
	}
}


void BFCPFloorControlServer::ProcessHello(BFCPMsgHello *req, BFCPUser* user)
{
	// TODO: tmp, for testing.
	SendMessage(req);
}


void BFCPFloorControlServer::SendMessage(BFCPMessage *msg)
{
	int userId = msg->GetUserId();

	BFCPUser* user = this->GetUser(userId);
	if (! user) {
		::Error("BFCPFloorControlServer::SendMessage() | user '%d' does not exist\n", userId);
		return;
	}

	::Debug("BFCPFloorControlServer::SendMessage() | sending message to user '%d':\n", userId);
	msg->Dump();

	user->SendMessage(msg);
}


void BFCPFloorControlServer::ReplyError(BFCPMessage *msg, BFCPAttrErrorCode::ErrorCode errorCode)
{
	::Log("BFCPFloorControlServer::ReplyError() | replying BFCP Error\n");
	BFCPMsgError *error_response = new BFCPMsgError(msg, errorCode);

	SendMessage(error_response);
	delete error_response;
}


void BFCPFloorControlServer::ReplyError(BFCPMessage *msg, BFCPAttrErrorCode::ErrorCode errorCode, std::wstring errorInfo)
{
	::Log("BFCPFloorControlServer::ReplyError() | replying BFCP Error\n");
	BFCPMsgError *error_response = new BFCPMsgError(msg, errorCode, errorInfo);

	SendMessage(error_response);
	delete error_response;
}
