#include "bfcp.h"
#include "bfcp/attributes/BFCPAttrErrorCode.h"
#include "bfcp/attributes/BFCPAttrRequestStatus.h"
#include "bfcp/messages/BFCPMsgError.h"
#include "stringparser.h"
#include "log.h"


// Initialize static members of BFCP related classes.
// Must be called just once.
void BFCP::Init()
{
	BFCPMessage::Init();
	BFCPAttribute::Init();
	// Init static data in specific attributes.
	BFCPAttrErrorCode::Init();
	BFCPAttrRequestStatus::Init();
}


BFCP::BFCP()
{

}


BFCPFloorControlServer* BFCP::CreateConference(int conferenceId, BFCPFloorControlServer::Listener *listener)
{
	// Ensure the conference does not already exist.
	if (this->GetConference(conferenceId)) {
		::Error("BFCP::CreateConference() | conference '%d' already exists\n", conferenceId);
		return NULL;
	}

	// Create the new conference instance.
	BFCPFloorControlServer *conference = new BFCPFloorControlServer(conferenceId, listener);

	// Add to the map.
	this->conferences[conferenceId] = conference;

	::Log("BFCP::CreateConference() | conference '%d' created\n", conferenceId);

	// Return the conference.
	return conference;
}


bool BFCP::RemoveConference(BFCPFloorControlServer* conference)
{
	return RemoveConference(conference->GetConferenceId());
}


bool BFCP::RemoveConference(int conferenceId)
{
	// Ensure the conference does exist in the map and remove it.
	BFCPFloorControlServer* conference = GetConference(conferenceId);
	if (! conference) {
		::Error("BFCP::RemoveConference() | conference '%d' does not exist\n", conferenceId);
		return false;
	}

	// Revoke floors, disconnect participants, etc.
	conference->End();

	// Delete the conference.
	delete conference;
	this->conferences.erase(conferenceId);

	::Log("BFCP::RemoveConference() | conference '%d' deleted\n", conferenceId);
	return true;
}


bool BFCP::ConnectTransport(WebSocket *ws, BFCPFloorControlServer* conference, int userId)
{

	int conferenceId = conference->GetConferenceId();

	// Notify the conference about the user connection. If it replies false reject it.
	// Note that the user should be added before to the conference via conference->AddUser().
	if (! conference->UserConnected(userId, ws)) {
		::Error("BFCP::ConnectTransport() | conference '%d' does not accept connection from user '%d' => HTTP 403\n", conferenceId, userId);
		ws->Reject(403, "Invalid User");
		return false;
	}

	// Associate the (now validated) userId to this connection.
	BFCP::ConnectionData *conn_data = new BFCP::ConnectionData(conferenceId, userId);
	ws->SetUserData((void*)conn_data);

	::Log("BFCP::ConnectTransport() | user '%d' connected to conference '%d'\n", userId, conferenceId);

	// Upgrade (so accept the WebSocket connection).
	ws->Accept(this);

	return true;
}


BFCPFloorControlServer* BFCP::GetConference(int conferenceId)
{
	BFCP::Conferences::iterator it = this->conferences.find(conferenceId);
	if (it != this->conferences.end())
		return it->second;
	else
		return NULL;
}


void BFCP::onWebSocketConnection(const HTTPRequest &request, WebSocket *ws)
{
	::Debug("BFCP::onWebSocketConnection() | WebSocket connection initiated for %s\n", request.GetRequestURI().c_str());

	int conferenceId;
	int announcedUserId;

	// Get the URL which must look (assuming conferenceId 1234 and userId 5678) as follows:
	//   /bfcp/1234/5678
	std::string url = request.GetRequestURI();
	StringParser parser(url);

	// Check the URL.
	if (! parser.MatchString("/bfcp")) {
		::Error("BFCP::onWebSocketConnection() | bad URL: no /bfcp => HTTP 400\n");
		ws->Reject(400, "Bad Request");
		return;
	}

	// Extract conferenceId and userId.
	if (! parser.ParseChar('/')) {
		::Error("BFCP::onWebSocketConnection() | bad URL: no /bfcp/ => HTTP 400\n");
		ws->Reject(400, "Bad Request");
		return;
	}
	if (! parser.ParseInteger()) {
		::Error("BFCP::onWebSocketConnection() | bad URL: no /bfcp/CONFERENCE_ID => HTTP 400\n");
		ws->Reject(400, "Bad Request");
		return;
	}
	conferenceId = (int)parser.GetIntegerValue();
	if (! parser.ParseChar('/')) {
		::Error("BFCP::onWebSocketConnection() | bad URL: no /bfcp/CONFERENCE_ID/ => HTTP 400\n");
		ws->Reject(400, "Bad Request");
		return;
	}
	if (! parser.ParseInteger()) {
		::Error("BFCP::onWebSocketConnection() | bad URL: no /bfcp/CONFERENCE_ID/USER_ID => HTTP 400\n");
		ws->Reject(400, "Bad Request");
		return;
	}
	announcedUserId = (int)parser.GetIntegerValue();

	// Ensure the conference does exist.
	BFCPFloorControlServer* conference = GetConference(conferenceId);
	if (! conference) {
		::Error("BFCP::onWebSocketConnection() | conference '%d' does not exist => HTTP 404\n", conferenceId);
		ws->Reject(404, "Conference Does Not Exist");
		return;
	}

	// Notify the conference about the user connection. If it replies false reject it.
	if (! conference->UserConnected(announcedUserId, ws)) {
		::Error("BFCP::onWebSocketConnection() | conference '%d' does not accept connection from user '%d' => HTTP 403\n", conferenceId, announcedUserId);
		ws->Reject(403, "Invalid User");
		return;
	}

	// Associate the (now validated) announced userId to this connection.
	BFCP::ConnectionData *conn_data = new BFCP::ConnectionData(conferenceId, announcedUserId);
	ws->SetUserData((void*)conn_data);

	// Upgrade (so accept the WebSocket connection).
	ws->Accept(this);
}


void BFCP::onOpen(WebSocket *ws)
{
	// I don't need to use this event (it is always fired after ws->Accept()).
}


void BFCP::onClose(WebSocket *ws)
{
	BFCP::ConnectionData *conn_data = (BFCP::ConnectionData *)ws->GetUserData();
	if (! conn_data)
		return;

	int conferenceId = conn_data->conferenceId;
	int userId = conn_data->userId;
	int closedByServer = conn_data->closedByServer;

	delete conn_data;

	BFCPFloorControlServer* conference = GetConference(conferenceId);
	if (! conference)
		return;

	::Debug("BFCP::onClose() | WebSocket connection closed [conferenceId:%d,userId:%d,closedByServer:%s]\n", conferenceId, userId, closedByServer ? "true" : "false");

	conference->UserDisconnected(userId, closedByServer);
}


void BFCP::onError(WebSocket *ws)
{
	BFCP::ConnectionData *conn_data = (BFCP::ConnectionData *)ws->GetUserData();

	::Error("BFCP::onError() | [conferenceId:%d,userId:%d,closedByServer:%s]\n", conn_data->conferenceId, conn_data->userId, conn_data->closedByServer ? "true" : "false");

	onClose(ws);
}


void BFCP::onMessageStart(WebSocket *ws, WebSocket::MessageType type, const DWORD length)
{
	BFCP::ConnectionData *conn_data = (BFCP::ConnectionData *)ws->GetUserData();

	conn_data->utf8.Reset();
	conn_data->utf8.SetSize(length);
}


void BFCP::onMessageData(WebSocket *ws, const BYTE* data, const DWORD size)
{
	BFCP::ConnectionData *conn_data = (BFCP::ConnectionData *)ws->GetUserData();

	conn_data->utf8.Parse(data,size);
}


void BFCP::onMessageEnd(WebSocket *ws)
{
	BFCP::ConnectionData *conn_data = (BFCP::ConnectionData *)ws->GetUserData();
	BFCPMessage* msg = NULL;
	BFCPFloorControlServer* conference = GetConference(conn_data->conferenceId);

	::Debug("BFCP::onMessageEnd() | received WebSocket message [conferenceId:%d,userId:%d]\n", conn_data->conferenceId, conn_data->userId);

	// Check that parsing went ok.
	if (! conn_data->utf8.IsParsed()) {
		::Error("BFCP::onMessageEnd() | invalid UTF8 in the WebSocket message [conferenceId:%d,userId:%d]\n", conn_data->conferenceId, conn_data->userId);

		if (conference) {
			conference->DisconnectUser(conn_data->userId, 1007, L"invalid UTF8 in the WebSocket message");
		}
		else {
			ws->Close(1007, L"invalid UTF8 in the WebSocket message");
		}

		return;
	}

	// Parse the message according to JSON BFCP syntax.
	msg = BFCPMessage::Parse(conn_data->utf8.GetWString());
	if (! msg) {
		::Error("BFCP::onMessageEnd() | invalid JSON BFCP message [conferenceId:%d,userId:%d]\n", conn_data->conferenceId, conn_data->userId);

		if (conference) {
			conference->DisconnectUser(conn_data->userId, 4003, L"invalid JSON BFCP message");
		}
		else {
			ws->Close(4003, L"invalid JSON BFCP message");
		}

		return;
	}

	// Check that the userId in the message matches the userId asociated to the WebSocket connection.
	if (msg->GetUserId() != conn_data->userId) {
		::Error("BFCP::onMessageEnd() | userId '%d' in received message does not match userId '%d' of the WS connection, replying BFCP Error 'UnauthorizedOperation'\n", msg->GetUserId(), conn_data->userId);

		BFCPMsgError *error_response = new BFCPMsgError(msg, BFCPAttrErrorCode::UnauthorizedOperation, L"userId in message does not match the userId for this connection");
		error_response->Dump();
		ws->SendMessage(error_response->Stringify());
		delete error_response;

		delete msg;

		if (conference) {
			conference->DisconnectUser(conn_data->userId, 4003, L"userId does not match the userId for this WebSocket connection");
		}
		else {
			ws->Close(4003, L"userId does not match the userId for this WebSocket connection");
		}

		return;
	}

	// Check that the conferenceId in the message matches the conferenceId asociated to the WebSocket connection.
	if (msg->GetConferenceId() != conn_data->conferenceId) {
		::Error("BFCP::onMessageEnd() | conferenceId '%d' in received message does not match conferenceId '%d' of the WS connection, replying BFCP Error 'UnauthorizedOperation'\n", msg->GetConferenceId(), conn_data->conferenceId);

		BFCPMsgError *error_response = new BFCPMsgError(msg, BFCPAttrErrorCode::UnauthorizedOperation, L"conferenceId in the message does not match the conferenceId for this connection");
		error_response->Dump();
		ws->SendMessage(error_response->Stringify());
		delete error_response;

		delete msg;

		if (conference) {
			conference->DisconnectUser(conn_data->userId, 4003, L"conferenceId does not match the conferenceId for this WebSocket connection");
		}
		else {
			ws->Close(4003, L"conferenceId does not match the conferenceId for this WebSocket connection");
		}

		return;
	}

	// Ensure the conference still exists.
	if (! conference) {
		::Error("BFCP::onMessageEnd() | conference '%d' does no longer exist, replying BFCP Error 'ConferenceDoesNotExist'\n", conn_data->conferenceId);

		BFCPMsgError *error_response = new BFCPMsgError(msg, BFCPAttrErrorCode::ConferenceDoesNotExist, L"the conference does no longer exist");
		error_response->Dump();
		ws->SendMessage(error_response->Stringify());
		delete error_response;

		delete msg;

		if (conference) {
			conference->DisconnectUser(conn_data->userId, 4003, L"conference does not longer exist");
		}
		else {
			ws->Close(4003, L"conference does not longer exist");
		}

		return;
	}

	// Pass the received BFCP message to the conference.
	conference->MessageReceived(msg);

	delete(msg);
}

void BFCP::onWriteBufferEmpty(WebSocket *ws)
{
	//Nothing
}
