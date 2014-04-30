#include "bfcp/attributes/BFCPAttrFloorRequestStatus.h"
#include "log.h"


/* Instance methods */

BFCPAttrFloorRequestStatus::BFCPAttrFloorRequestStatus(int floorId) :
	floorId(new BFCPAttrFloorId(floorId)),
	requestStatus(NULL)
{
}


BFCPAttrFloorRequestStatus::~BFCPAttrFloorRequestStatus()
{
	::Debug("BFCPAttrFloorRequestStatus::~BFCPAttrFloorRequestStatus() | free memory\n");

	delete this->floorId;
	if (this->requestStatus)
		delete this->requestStatus;
}


void BFCPAttrFloorRequestStatus::Dump()
{
	::Debug("[BFCPAttrFloorRequestStatus]\n");
	::Debug("- floorId: %d\n", this->floorId->GetValue());
	if (this->requestStatus) {
		::Debug("- requestStatus:\n");
		this->requestStatus->Dump();
	}
	::Debug("[/BFCPAttrFloorRequestStatus]\n");
}


void BFCPAttrFloorRequestStatus::Stringify(std::wstringstream &json_stream)
{
	json_stream << L"{";
	json_stream << L"\n  \"floorId\": " << this->floorId->GetValue();
	if (this->requestStatus) {
		json_stream << L",\n  \"requestStatus\": ";
		this->requestStatus->Stringify(json_stream);
	}
	json_stream << L"\n}";
}


void BFCPAttrFloorRequestStatus::SetRequestStatus(BFCPAttrRequestStatus *requestStatus)
{
	if (this->requestStatus)
		delete this->requestStatus;

	this->requestStatus = requestStatus;
}
