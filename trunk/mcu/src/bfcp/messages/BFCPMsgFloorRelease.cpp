#include "bfcp/messages/BFCPMsgFloorRelease.h"
#include "log.h"


/* Instance members. */

BFCPMsgFloorRelease::BFCPMsgFloorRelease(int transactionId, int conferenceId, int userId) :
		BFCPMessage(BFCPMessage::FloorRelease, transactionId, conferenceId, userId),
		floorRequestId(NULL)
{
}


BFCPMsgFloorRelease::~BFCPMsgFloorRelease()
{
	::Debug("BFCPMsgFloorRelease::~BFCPMsgFloorRelease() | free memory\n");

	if (this->floorRequestId)
		delete this->floorRequestId;
}


bool BFCPMsgFloorRelease::ParseAttributes(JSONParser &parser)
{
	do {
		// Get key name.
		if (! parser.ParseJSONString()) {
			::Error("BFCPMsgFloorRelease::ParseAttributes() | failed to parse JSON key\n");
			goto error;
		}

		enum BFCPAttribute::Name attribute = BFCPAttribute::mapJsonStr2Name[parser.GetValue()];
		//::Log("BFCPMsgFloorRelease::ParseAttributes() | BFCPAttribute::Name attribute: %d\n", attribute);

		if (! parser.ParseDoubleDot()) {
			::Error("BFCPMsgFloorRelease::ParseAttributes() | failed to parse ':'\n");
			goto error;
		}

		switch(attribute) {
			case BFCPAttribute::FloorRequestId:
				if (! parser.ParseJSONNumber()) {
					::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse 'floorRequestId'\n");
					goto error;
				}

				::Debug("BFCPMsgFloorRequest::ParseAttributes() | attribute 'floorRequestId' found\n");
				SetFloorRequestId((int)parser.GetNumberValue());
				break;

			default:
				::Debug("BFCPMsgFloorRelease::ParseAttributes() | skiping unknown key\n");
				parser.SkipJSONValue();
				break;
		}
	} while (parser.ParseComma());

	::Debug("BFCPMsgFloorRelease::ParseAttributes() | exiting attributes object\n");
	return true;

error:
	return false;
}


bool BFCPMsgFloorRelease::IsValid()
{
	// Must have a FloorRequestId.
	if (! this->floorRequestId) {
		::Error("BFCPMsgFloorRelease::IsValid() | MUST have a FloorRequestId\n");
		return false;
	}

	return true;
}


void BFCPMsgFloorRelease::Dump()
{
	::Debug("[BFCPMsgFloorRelease]\n");
	::Debug("- primitive: FloorRelease\n");
	::Debug("- common header: [conferenceId: %d, userId: %d, transactionId: %d]\n", this->conferenceId, this->userId, this->transactionId);
	::Debug("- floodRequestId: %d\n", GetFloorRequestId());
	::Debug("[/BFCPMsgFloorRelease]\n");
}


void BFCPMsgFloorRelease::SetFloorRequestId(int floorRequestId)
{
	// If already set, replace it.
	if (this->floorRequestId) {
		::Debug("BFCPMsgFloorRelease::SetFloorRequestId() | attribute 'floorRequestId' was already set, replacing it\n");
		delete this->floorRequestId;
	}
	this->floorRequestId = new BFCPAttrFloorRequestId(floorRequestId);
}


int BFCPMsgFloorRelease::GetFloorRequestId()
{
	// NOTE: This should be not possible given that this is validated during parsing.
	if (! this->floorRequestId)
		throw BFCPMessage::AttributeNotFound("'floorRequestId' attribute not found");

	return this->floorRequestId->GetValue();
}

