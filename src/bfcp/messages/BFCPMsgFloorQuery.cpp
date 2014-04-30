#include "bfcp/messages/BFCPMsgFloorQuery.h"
#include "log.h"


/* Instance members. */

BFCPMsgFloorQuery::BFCPMsgFloorQuery(int transactionId, int conferenceId, int userId) :
		BFCPMessage(BFCPMessage::FloorQuery, transactionId, conferenceId, userId)
{
}


BFCPMsgFloorQuery::~BFCPMsgFloorQuery()
{
	::Debug("BFCPMsgFloorQuery::~BFCPMsgFloorQuery() | free memory\n");

	int num_floors = this->floorIds.size();
	for (int i=0; i < num_floors; i++)
		delete this->floorIds[i];
}


bool BFCPMsgFloorQuery::ParseAttributes(JSONParser &parser)
{
	do {
		// Get key name.
		if (! parser.ParseJSONString()) {
			::Error("BFCPMsgFloorQuery::ParseAttributes() | failed to parse JSON key\n");
			goto error;
		}

		enum BFCPAttribute::Name attribute = BFCPAttribute::mapJsonStr2Name[parser.GetValue()];

		if (! parser.ParseDoubleDot()) {
			::Error("BFCPMsgFloorQuery::ParseAttributes() | failed to parse ':'\n");
			goto error;
		}

		switch(attribute) {
			case BFCPAttribute::FloorId:
				if (! parser.ParseJSONArrayStart()) {
					::Error("BFCPMsgFloorQuery::ParseAttributes() | failed to parse 'floorId' array start\n");
					goto error;
				}
				if (parser.ParseJSONArrayEnd())
					break;

				do {
					if (! parser.ParseJSONNumber()) {
						::Error("BFCPMsgFloorQuery::ParseAttributes() | failed to parse 'floorId' value in the array\n");
						goto error;
					}

					int floorId = (int)parser.GetNumberValue();
					::Debug("BFCPMsgFloorQuery::ParseAttributes() | attribute 'floorId' found\n");
					AddFloorId(floorId);
				} while (parser.ParseComma());

				if (! parser.ParseJSONArrayEnd()) {
					::Error("BFCPMsgFloorQuery::ParseAttributes() | failed to parse 'floorId' array end\n");
					goto error;
				}
				break;

			default:
				::Debug("BFCPMsgFloorQuery::ParseAttributes() | skiping unknown key\n");
				parser.SkipJSONValue();
				break;
		}
	} while (parser.ParseComma());

	::Debug("BFCPMsgFloorQuery::ParseAttributes() | exiting attributes object\n");
	return true;

error:
	return false;
}


bool BFCPMsgFloorQuery::IsValid()
{
	// Always valid.
	return true;
}


void BFCPMsgFloorQuery::Dump()
{
	::Debug("[BFCPMsgFloorQuery]\n");
	::Debug("- [primitive: FloorQuery, conferenceId: %d, userId: %d, transactionId: %d]\n", this->conferenceId, this->userId, this->transactionId);
	int num_floors = this->floorIds.size();
	for (int i=0; i < num_floors; i++) {
		::Debug("- floodId: %d\n", GetFloorId(i));
	}
	::Debug("[/BFCPMsgFloorQuery]\n");
}


void BFCPMsgFloorQuery::AddFloorId(int floorId)
{
	this->floorIds.push_back(new BFCPAttrFloorId(floorId));
}


int BFCPMsgFloorQuery::GetFloorId(unsigned int index)
{
	if (index >= this->floorIds.size())
		throw BFCPMessage::AttributeNotFound("'floorId' attribute not in range");

	return this->floorIds[index]->GetValue();
}


int BFCPMsgFloorQuery::CountFloorIds()
{
	return this->floorIds.size();
}
