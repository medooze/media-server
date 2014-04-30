#ifndef BFCPFLOORREQUEST_H
#define BFCPFLOORREQUEST_H


#include "bfcp/messages/BFCPMsgFloorRequestStatus.h"
#include <set>
#include <string>


class BFCPFloorRequest
{
public:
	BFCPFloorRequest(int floorRequestId, int userId, int conferenceId);

	void SetBeneficiaryId(int beneficiaryId);
	void AddFloorId(int floorId);
	void SetStatus(enum BFCPAttrRequestStatus::Status status);
	void SetQueuePosition(int queuePosition);
	bool HasFloorId(int floorId);
	int GetFloorRequestId();
	int GetUserId();
	int GetBeneficiaryId();
	std::set<int> GetFloorIds();
	enum BFCPAttrRequestStatus::Status GetStatus();
	std::wstring GetStatusString();
	bool IsGranted();
	bool CanBeGranted();
	bool CanBeDenied();
	bool CanBeCancelled();
	int GetQueuePosition();
	void Dump();

	// Generates a FloorRequestStatus response.
	BFCPMsgFloorRequestStatus* CreateFloorRequestStatus(int transactionId);
	// Generates a FloorRequestStatus notification (transactionId = 0).
	BFCPMsgFloorRequestStatus* CreateFloorRequestStatus();
	// Generates a FloorRequestInformation attribute.
	BFCPAttrFloorRequestInformation* CreateFloorRequestInformation();

private:
	int floorRequestId;
	int userId;
	int conferenceId;
	int beneficiaryId;
	std::set<int> floorIds;
	enum BFCPAttrRequestStatus::Status status;
	int queuePosition;
};


#endif
