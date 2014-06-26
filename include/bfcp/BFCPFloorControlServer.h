#ifndef BFCPFLOORCONTROLSERVER_H
#define BFCPFLOORCONTROLSERVER_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/messages.h"
#include "bfcp/attributes.h"
#include "bfcp/BFCPUser.h"
#include "bfcp/BFCPFloorRequest.h"
#include "websocketconnection.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include "use.h"


/**
 * A BFCPFloorControlServer handles a BFCP conference.
 */
class BFCPFloorControlServer
{
public:
	/**
	 * When a media conference takes place it may create a conference and insert itself as listener,
	 * so the media conference becomes a "BFCP chair".
	 */
	class Listener
	{
	public:

		/**
		 * Event called when a FloorRequest is received.
		 *
		 * The chair MUST later call
		 * BFCPFloorControlServer::GrantFloorRequest() or BFCPFloorControlServer::DenyFloorRequest()
		 * by passing the same floorRequestId as argument.
		 *
		 * @param floorRequestId  The FloorRequest identifier.
		 * @param userId          The sender of the FloorRequest.
		 * @param beneficiaryId   The beneficiary of the FloorRequest (may be same as userId).
		 * @param floorIds        The floors being requested.
		 */
		virtual void onFloorRequest(int floorRequestId, int userId, int beneficiaryId, std::set<int> floorIds) = 0;

		/**
		 * This event is called when the BFCP Floor Control Server has granted a FloorRequest
		 * (so after the chair called to BFCPFloorControlServer::GrantFloorRequest(), but
		 * may happen some time later).
		 *
		 * This is just a notification for the chair.
		 *
		 * @param floorRequestId  The FloorRequest identifier.
		 * @param beneficiaryId   The user currently owning the floors.
		 * @param floorIds        The floors of the associated FloorRequest.
		 */
		virtual void onFloorGranted(int floorRequestId, int beneficiaryId, std::set<int> floorIds) = 0;

		/**
		 * This event is called when the BFCP Floor Control Server has released/revoked a FloorRelease
		 * (so after the chair called to BFCPFloorControlServer::RejectFloorRelease() if it
		 * was requested by a different user than the original FloorRequest, or after the chair
		 * called to BFCPFloorControlServer::RevokeFloorRequest(), or after the owner
		 * itself released it, which requires no permission from the chair).
		 *
		 * This is just a notification for the receiver.
		 *
		 * @param floorRequestId  The FloorRequest identifier.
		 * @param beneficiaryId   The user that was owing the floors until now.
		 * @param floorIds        The floors of the associated FloorRequest.
		 */
		virtual void onFloorReleased(int floorRequestId, int beneficiaryId, std::set<int> floorIds) = 0;
	};

public:
	BFCPFloorControlServer(int conferenceId, Listener *listener);
	~BFCPFloorControlServer();

	int GetConferenceId();

	// Called by chair.
	bool AddUser(int userId);
	bool RemoveUser(int userId);
	bool SetChair(int userId);
	bool AddFloor(int floorId);
	bool GrantFloorRequest(int floorRequestId);
	bool DenyFloorRequest(int floorRequestId, std::wstring statusInfo = L"");
	bool RevokeFloorRequest(int floorRequestId, std::wstring statusInfo = L"");
	// Returns 0 if there is no a Granted FloorRequest owning the given floor.
	int GetGrantedFloorRequestId(int floorId);
	void End();

	// Called by the transport.
	bool UserConnected(int announcedUserId, WebSocket *ws);
	void UserDisconnected(int userId, bool revoked);
	void DisconnectUser(int userId, const WORD code, const std::wstring& reason);
	void MessageReceived(BFCPMessage *msg);

private:
	bool HasUser(int userId);
	BFCPUser* GetUser(int userId);
	BFCPFloorRequest* GetFloorRequest(int floorRequestId);
	std::vector<BFCPFloorRequest*> GetFloorRequestsForFloor(int floorId);
	std::vector<BFCPFloorRequest*> GetFloorRequestsForFloors(std::set<int> floorIds);
	void NotifyForFloorRequest(BFCPFloorRequest* floorRequest);
	void RevokeUserFloorRequests(BFCPUser* user);
	bool HasFloor(int floorId);
	void ProcessFloorRequest(BFCPMsgFloorRequest *req, BFCPUser* user);
	void ProcessFloorRelease(BFCPMsgFloorRelease *req, BFCPUser* user);
	void ProcessFloorQuery(BFCPMsgFloorQuery *req, BFCPUser* user);
	void ProcessHello(BFCPMsgHello *req, BFCPUser* user);
	void SendMessage(BFCPMessage *msg);
	void ReplyError(BFCPMessage *msg, BFCPAttrErrorCode::ErrorCode errorCode);
	void ReplyError(BFCPMessage *msg, BFCPAttrErrorCode::ErrorCode errorCode, std::wstring errorInfo);

private:
	typedef UseMap<int, BFCPUser*> Users;
	typedef std::set<int> Floors;
	typedef UseMap<int, BFCPFloorRequest*> FloorRequests;
	// A vector to store removed users.
	typedef std::vector<BFCPUser*> RemovedUsers;

private:
	Listener *listener;
	int conferenceId;
	Users users;
	RemovedUsers removedUsers;
	Floors floors;
	FloorRequests floorRequests;
	int floorRequestCounter;
	bool ending;
};


#endif /* BFCPFLOORCONTROLSERVER_H */
