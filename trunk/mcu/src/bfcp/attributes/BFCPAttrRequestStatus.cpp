#include "bfcp/attributes/BFCPAttrRequestStatus.h"
#include "log.h"


// Initialize static members of the class.
std::map<enum BFCPAttrRequestStatus::Status, std::wstring>  BFCPAttrRequestStatus::mapStatus2JsonStr;


/* Static methods */

void BFCPAttrRequestStatus::Init()
{
	mapStatus2JsonStr[Pending] = L"Pending";
	mapStatus2JsonStr[Accepted] = L"Accepted";
	mapStatus2JsonStr[Granted] = L"Granted";
	mapStatus2JsonStr[Denied] = L"Denied";
	mapStatus2JsonStr[Cancelled] = L"Cancelled";
	mapStatus2JsonStr[Released] = L"Released";
	mapStatus2JsonStr[Revoked] = L"Revoked";
}


/* Instance methods */

BFCPAttrRequestStatus::BFCPAttrRequestStatus() :
	status(BFCPAttrRequestStatus::Pending),
	queuePosition(0)
{
}


BFCPAttrRequestStatus::BFCPAttrRequestStatus(enum BFCPAttrRequestStatus::Status status, int queuePosition) :
	status(status),
	queuePosition(queuePosition)
{
}


void BFCPAttrRequestStatus::Dump()
{
	::Debug("[BFCPAttrRequestStatus]\n");
	::Debug("- status: %ls\n", GetStatusString().c_str());
	::Debug("- queuePosition: %d\n", this->queuePosition);
	::Debug("[/BFCPAttrRequestStatus]\n");
}


void BFCPAttrRequestStatus::Stringify(std::wstringstream &json_stream)
{
	json_stream << L"{";
	json_stream << L"\n  \"status\": \"" << GetStatusString().c_str() << L"\"";
	json_stream << L",\n  \"queuePosition\": " << this->queuePosition;
	json_stream << L"\n}";
}


void BFCPAttrRequestStatus::SetStatus(enum BFCPAttrRequestStatus::Status status)
{
	this->status = status;
}


void BFCPAttrRequestStatus::SetQueuePosition(int queuePosition)
{
	this->queuePosition = queuePosition;
}


enum BFCPAttrRequestStatus::Status BFCPAttrRequestStatus::GetStatus()
{
	return this->status;
}


std::wstring BFCPAttrRequestStatus::GetStatusString()
{
	return BFCPAttrRequestStatus::mapStatus2JsonStr[this->status];
}


int BFCPAttrRequestStatus::GetQueuePosition()
{
	return this->queuePosition;
}
