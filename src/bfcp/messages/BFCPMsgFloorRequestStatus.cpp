#include "bfcp/messages/BFCPMsgFloorRequestStatus.h"
#include "log.h"


/* Instance members. */

BFCPMsgFloorRequestStatus::BFCPMsgFloorRequestStatus(int transactionId, int conferenceId, int userId) :
		BFCPMessage(BFCPMessage::FloorRequestStatus, transactionId, conferenceId, userId),
		floorRequestInformation(NULL)
{
}


BFCPMsgFloorRequestStatus::~BFCPMsgFloorRequestStatus()
{
	::Debug("BFCPMsgFloorRequestStatus::~BFCPMsgFloorRequestStatus() | free memory\n");

	if (this->floorRequestInformation)
		delete this->floorRequestInformation;
}


void BFCPMsgFloorRequestStatus::Dump()
{
	::Debug("[BFCPMsgFloorRequestStatus]\n");
	::Debug("- [primitive: FloorRequestStatus, conferenceId: %d, userId: %d, transactionId: %d]\n", this->conferenceId, this->userId, this->transactionId);
	if (this->floorRequestInformation) {
		::Debug("- floorRequestInformation:\n");
		this->floorRequestInformation->Dump();
	}
	::Debug("[/BFCPMsgFloorRequestStatus]\n");
}


std::wstring BFCPMsgFloorRequestStatus::Stringify() {
	std::wstringstream json_stream;

	json_stream << L"{";
	StringifyCommonHeader(json_stream);

	// Attributes.
	json_stream << L",";
	json_stream << L"\n\"attributes\": {";

	// floorRequestInformation.
	if (this->floorRequestInformation) {
		json_stream << L"\n  \"floorRequestInformation\": ";
		this->floorRequestInformation->Stringify(json_stream);
	}

	// End of attributes.
	json_stream << L"\n}";

	// End of root object.
	json_stream << L"\n}";

	return json_stream.str();
}


void BFCPMsgFloorRequestStatus::SetFloorRequestInformation(BFCPAttrFloorRequestInformation *floorRequestInformation)
{
	if (this->floorRequestInformation)
		delete this->floorRequestInformation;

	this->floorRequestInformation = floorRequestInformation;
}


void BFCPMsgFloorRequestStatus::SetDescription(std::wstring& statusInfo)
{
	if (! this->floorRequestInformation)
		return;

	this->floorRequestInformation->SetDescription(statusInfo);
}
