#include "bfcp/attributes/BFCPAttrOverallRequestStatus.h"
#include "log.h"


/* Instance methods */

BFCPAttrOverallRequestStatus::BFCPAttrOverallRequestStatus(int floorRequestId) :
	floorRequestId(new BFCPAttrFloorRequestId(floorRequestId)),
	requestStatus(NULL),
	statusInfo(NULL)
{
}


BFCPAttrOverallRequestStatus::~BFCPAttrOverallRequestStatus()
{
	::Debug("BFCPAttrOverallRequestStatus::~BFCPAttrOverallRequestStatus() | free memory\n");

	delete this->floorRequestId;
	if (this->requestStatus)
		delete this->requestStatus;
	if (this->statusInfo)
		delete this->statusInfo;
}


void BFCPAttrOverallRequestStatus::Dump()
{
	::Debug("[BFCPAttrOverallRequestStatus]\n");
	::Debug("- floorRequestId: %d\n", this->floorRequestId->GetValue());
	if (this->requestStatus) {
		::Debug("- requestStatus:\n");
		this->requestStatus->Dump();
	}
	if (this->statusInfo) {
		::Debug("- statusInfo: (not shown)\n");
	}
	::Debug("[/BFCPAttrOverallRequestStatus]\n");
}


void BFCPAttrOverallRequestStatus::Stringify(std::wstringstream &json_stream)
{
	json_stream << L"{";
	json_stream << L"\n  \"floorRequestId\": " << this->floorRequestId->GetValue();
	if (this->requestStatus) {
		json_stream << L",\n  \"requestStatus\": ";
		this->requestStatus->Stringify(json_stream);
	}
	if (this->statusInfo) {
		json_stream << L",\n  \"statusInfo\": \"" << this->statusInfo->GetValue() << L"\"";
	}
	json_stream << L"\n}";
}


void BFCPAttrOverallRequestStatus::SetRequestStatus(BFCPAttrRequestStatus *requestStatus)
{
	if (this->requestStatus)
		delete this->requestStatus;
	this->requestStatus = requestStatus;
}


void BFCPAttrOverallRequestStatus::SetStatusInfo(BFCPAttrStatusInfo *statusInfo)
{
	if (this->statusInfo)
		delete this->statusInfo;
	this->statusInfo = statusInfo;
}
