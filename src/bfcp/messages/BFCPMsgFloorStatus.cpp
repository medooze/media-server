#include "bfcp/messages/BFCPMsgFloorStatus.h"
#include "log.h"
#include <algorithm>


/* Instance members. */

BFCPMsgFloorStatus::BFCPMsgFloorStatus(int transactionId, int conferenceId, int userId) :
		BFCPMessage(BFCPMessage::FloorStatus, transactionId, conferenceId, userId),
		floorId(NULL)
{
}


BFCPMsgFloorStatus::~BFCPMsgFloorStatus()
{
	::Debug("BFCPMsgFloorStatus::~BFCPMsgFloorStatus() | free memory\n");

	if (this->floorId)
		delete this->floorId;
	int num_floor_request_informations = this->floorRequestInformations.size();
	for (int i=0; i < num_floor_request_informations; i++)
		delete this->floorRequestInformations[i];
}


void BFCPMsgFloorStatus::Dump()
{
	::Debug("[BFCPMsgFloorStatus]\n");
	::Debug("- [primitive: FloorStatus, conferenceId: %d, userId: %d, transactionId: %d]\n", this->conferenceId, this->userId, this->transactionId);
	if (this->floorId) {
		::Debug("- floorId:\n", this->floorId->GetValue());
	}
	int num_floor_request_informations = this->floorRequestInformations.size();
	for (int i=0; i < num_floor_request_informations; i++) {
		::Debug("- floorRequestInformation:\n");
		this->floorRequestInformations[i]->Dump();
	}
	::Debug("[/BFCPMsgFloorStatus]\n");
}


std::wstring BFCPMsgFloorStatus::Stringify() {
	std::wstringstream json_stream;

	json_stream << L"{";
	StringifyCommonHeader(json_stream);

	// Attributes.
	json_stream << L",";
	json_stream << L"\n\"attributes\": {";

	// floorId.
	if (this->floorId) {
		json_stream << L"\n  \"floorId\": " << this->floorId->GetValue() << L",";
	}

	// floorRequestInformations.
	json_stream << L"\n  \"floorRequestInformation\": [";
	int num_floor_request_informations = this->floorRequestInformations.size();
	for (int i=0; i < num_floor_request_informations; i++) {
		this->floorRequestInformations[i]->Stringify(json_stream);
		if (i < num_floor_request_informations - 1)
			json_stream << L", ";
	}
	json_stream << L"  ]";

	// End of attributes.
	json_stream << L"\n}";

	// End of root object.
	json_stream << L"\n}";

	return json_stream.str();
}


void BFCPMsgFloorStatus::SetFloorId(int floorId)
{
	if (this->floorId)
		delete this->floorId;

	this->floorId = new BFCPAttrFloorId(floorId);
}


void BFCPMsgFloorStatus::AddFloorRequestInformation(BFCPAttrFloorRequestInformation* floorRequestInformation)
{
	// Avoid duplicated floorRequestInformations.
	if (std::find(this->floorRequestInformations.begin(), this->floorRequestInformations.end(), floorRequestInformation) != this->floorRequestInformations.end())
		return;

	this->floorRequestInformations.push_back(floorRequestInformation);
}


int BFCPMsgFloorStatus::GetFloorId()
{
	if (! this->floorId)
		throw BFCPMessage::AttributeNotFound("'floorId' attribute not found");

	return this->floorId->GetValue();
}
