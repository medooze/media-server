#include "bfcp/BFCPFloorRequest.h"
#include "bfcp/attributes/BFCPAttrFloorRequestInformation.h"
#include "bfcp/attributes/BFCPAttrFloorRequestStatus.h"
#include "bfcp/attributes/BFCPAttrOverallRequestStatus.h"
#include "bfcp/attributes/BFCPAttrRequestStatus.h"
#include "bfcp/attributes/BFCPAttrBeneficiaryInformation.h"
#include "bfcp/attributes/BFCPAttrRequestedByInformation.h"
#include "log.h"


/* Instance methods. */


BFCPFloorRequest::BFCPFloorRequest(int floorRequestId, int userId, int conferenceId) :
	floorRequestId(floorRequestId),
	userId(userId),
	conferenceId(conferenceId),
	beneficiaryId(userId),  // By the default beneficiaryId = userId.
	status(BFCPAttrRequestStatus::Pending),
	queuePosition(0)
{
}


void BFCPFloorRequest::SetBeneficiaryId(int beneficiaryId)
{
	this->beneficiaryId = beneficiaryId;
}


void BFCPFloorRequest::AddFloorId(int floorId)
{
	this->floorIds.insert(floorId);
}


void BFCPFloorRequest::SetStatus(enum BFCPAttrRequestStatus::Status status)
{
	this->status = status;
}


void BFCPFloorRequest::SetQueuePosition(int queuePosition)
{
	this->queuePosition = queuePosition;
}


bool BFCPFloorRequest::HasFloorId(int floorId)
{
	return (this->floorIds.find(floorId) != this->floorIds.end()) ? true : false;
}


int BFCPFloorRequest::GetFloorRequestId()
{
	return this->floorRequestId;
}


int BFCPFloorRequest::GetUserId()
{
	return this->userId;
}


int BFCPFloorRequest::GetBeneficiaryId()
{
	return this->beneficiaryId;
}


std::set<int> BFCPFloorRequest::GetFloorIds()
{
	return this->floorIds;
}


enum BFCPAttrRequestStatus::Status BFCPFloorRequest::GetStatus()
{
	return this->status;
}


std::wstring BFCPFloorRequest::GetStatusString()
{
	return BFCPAttrRequestStatus::mapStatus2JsonStr[this->status];
}


bool BFCPFloorRequest::IsGranted()
{
	return(this->status == BFCPAttrRequestStatus::Granted ? true : false);
}


bool BFCPFloorRequest::CanBeGranted()
{
	switch(this->status)
	{
		case BFCPAttrRequestStatus::Pending:
		case BFCPAttrRequestStatus::Accepted:
			return true;
			break;
		default:
			return false;
			break;
	}
}


bool BFCPFloorRequest::CanBeDenied()
{
	switch(this->status)
	{
		case BFCPAttrRequestStatus::Pending:
		case BFCPAttrRequestStatus::Accepted:
			return true;
			break;
		default:
			return false;
			break;
	}
}


bool BFCPFloorRequest::CanBeCancelled()
{
	switch(this->status)
	{
		case BFCPAttrRequestStatus::Pending:
		case BFCPAttrRequestStatus::Accepted:
			return true;
			break;
		default:
			return false;
			break;
	}
}


int BFCPFloorRequest::GetQueuePosition()
{
	return this->queuePosition;
}


void BFCPFloorRequest::Dump()
{
	::Debug("[BFCPFloorRequest]\n");
	::Debug("- floorRequestId: %d\n", this->floorRequestId);
	::Debug("- userId: %d\n", this->userId);
	::Debug("- conferenceId: %d\n", this->conferenceId);
	::Debug("- beneficiaryId: %d\n", this->beneficiaryId);
	std::set<int>::iterator it;
	for (it = this->floorIds.begin(); it != this->floorIds.end(); ++it) {
		::Debug("- floorId: %d\n", *it);
	}
	::Debug("- status: %ls\n", GetStatusString().c_str());
	::Debug("- queuePosition: %d\n", this->queuePosition);
	::Debug("[/BFCPFloorRequest]\n");
}


BFCPMsgFloorRequestStatus* BFCPFloorRequest::CreateFloorRequestStatus(int transactionId)
{
	::Debug("BFCPFloorRequest::CreateFloorRequestStatus() | [floorRequestId:%d,transactionId:%d,conferenceId:%d,status:%ls,queuePosition:%d]\n", this->floorRequestId, transactionId, this->conferenceId, BFCPAttrRequestStatus::mapStatus2JsonStr[this->status].c_str(), this->queuePosition);

	// Build a FloorRequestStatus message.
	BFCPMsgFloorRequestStatus *floorRequestStatus = new BFCPMsgFloorRequestStatus(transactionId, this->conferenceId, this->userId);

	// Apppend a FloorRequestInformation attribute to the FloorRequestStatus message.
	BFCPAttrFloorRequestInformation *floorRequestInformation = CreateFloorRequestInformation();
	floorRequestStatus->SetFloorRequestInformation(floorRequestInformation);

	return floorRequestStatus;
}


BFCPMsgFloorRequestStatus* BFCPFloorRequest::CreateFloorRequestStatus()
{
	CreateFloorRequestStatus(0);
}


BFCPAttrFloorRequestInformation* BFCPFloorRequest::CreateFloorRequestInformation()
{
	::Debug("BFCPFloorRequest::CreateFloorRequestInformation() | [floorRequestId:%d,status:%ls,queuePosition:%d]\n", this->floorRequestId, BFCPAttrRequestStatus::mapStatus2JsonStr[this->status].c_str(), this->queuePosition);

	BFCPAttrFloorRequestInformation *floorRequestInformation = new BFCPAttrFloorRequestInformation(this->floorRequestId);

	// Append as many FloorRequestStatus attributes to the FloorRequestInformation attribute as floorIds.
	std::set<int>::iterator it;
	for (it = this->floorIds.begin(); it != this->floorIds.end(); ++it) {
		BFCPAttrFloorRequestStatus *floorRequestStatus = new BFCPAttrFloorRequestStatus(*it);
		floorRequestInformation->AddFloorRequestStatus(floorRequestStatus);

		// Append a RequestStatus attribute to each FloorRequestStatus attribute.
		BFCPAttrRequestStatus *requestStatus = new BFCPAttrRequestStatus(this->status, this->queuePosition);
		floorRequestStatus->SetRequestStatus(requestStatus);
	}

	// Append a OverallRequestStatus attributes to the FloorRequestInformation attribute.
	BFCPAttrOverallRequestStatus* overallRequestStatus = new BFCPAttrOverallRequestStatus(this->floorRequestId);
	floorRequestInformation->SetOverallRequestStatus(overallRequestStatus);

	// Append a RequestStatus attribute to the OverallRequestStatus attribute.
	BFCPAttrRequestStatus* requestStatus = new BFCPAttrRequestStatus(this->status, this->queuePosition);
	overallRequestStatus->SetRequestStatus(requestStatus);

	// Append a BeneficiaryInformation attribute to the FloorRequestInformation attribute.
	BFCPAttrBeneficiaryInformation *beneficiaryInformation = new BFCPAttrBeneficiaryInformation(this->beneficiaryId);
	floorRequestInformation->SetBeneficiaryInformation(beneficiaryInformation);

	// Append a RequestedByInformation attribute to the FloorRequestInformation attribute.
	BFCPAttrRequestedByInformation *requestedByInformation = new BFCPAttrRequestedByInformation(this->userId);
	floorRequestInformation->SetRequestedByInformation(requestedByInformation);

	return floorRequestInformation;
}
