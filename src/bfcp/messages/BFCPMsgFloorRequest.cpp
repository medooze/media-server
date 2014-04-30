#include "bfcp/messages/BFCPMsgFloorRequest.h"
#include "log.h"


/* Instance members. */

BFCPMsgFloorRequest::BFCPMsgFloorRequest(int transactionId, int conferenceId, int userId) :
		BFCPMessage(BFCPMessage::FloorRequest, transactionId, conferenceId, userId),
		beneficiaryId(NULL),
		participantProvidedInfo(NULL)
{
}


BFCPMsgFloorRequest::~BFCPMsgFloorRequest()
{
	::Debug("BFCPMsgFloorRequest::~BFCPMsgFloorRequest() | free memory\n");

	int num_floors = this->floorIds.size();
	for (int i=0; i < num_floors; i++)
		delete this->floorIds[i];
	if (this->beneficiaryId)
		delete this->beneficiaryId;
	if (this->participantProvidedInfo)
		delete this->participantProvidedInfo;
}


bool BFCPMsgFloorRequest::ParseAttributes(JSONParser &parser)
{
	do {
		// Get key name.
		if (! parser.ParseJSONString()) {
			::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse JSON key\n");
			goto error;
		}

		enum BFCPAttribute::Name attribute = BFCPAttribute::mapJsonStr2Name[parser.GetValue()];
		//::Log("BFCPMsgFloorRequest::ParseAttributes() | BFCPAttribute::Name attribute: %d\n", attribute);

		if (! parser.ParseDoubleDot()) {
			::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse ':'\n");
			goto error;
		}

		switch(attribute) {
			case BFCPAttribute::FloorId:
				if (! parser.ParseJSONArrayStart()) {
					::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse 'floorId' array start\n");
					goto error;
				}
				if (parser.ParseJSONArrayEnd())
					break;

				do {
					if (! parser.ParseJSONNumber()) {
						::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse 'floorId' value in the array\n");
						goto error;
					}

					int floorId = (int)parser.GetNumberValue();
					::Debug("BFCPMsgFloorRequest::ParseAttributes() | attribute 'floorId' found\n");
					AddFloorId(floorId);
				} while (parser.ParseComma());

				if (! parser.ParseJSONArrayEnd()) {
					::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse 'floorId' array end\n");
					goto error;
				}
				break;

			case BFCPAttribute::BeneficiaryId:
				if (! parser.ParseJSONNumber()) {
					::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse 'beneficiaryId'\n");
					goto error;
				}

				::Debug("BFCPMsgFloorRequest::ParseAttributes() | attribute 'beneficiaryId' found\n");
				SetBeneficiaryId((int)parser.GetNumberValue());
				break;

			case BFCPAttribute::ParticipantProvidedInfo:
				if (! parser.ParseJSONString()) {
					::Error("BFCPMsgFloorRequest::ParseAttributes() | failed to parse 'participantProvidedInfo'\n");
					goto error;
				}

				::Debug("BFCPMsgFloorRequest::ParseAttributes() | attribute 'participantProvidedInfo' found\n");
				SetParticipantProvidedInfo(parser.GetValue());
				break;

			default:
				::Debug("BFCPMsgFloorRequest::ParseAttributes() | skiping unknown key\n");
				parser.SkipJSONValue();
				break;
		}
	} while (parser.ParseComma());

	::Debug("BFCPMsgFloorRequest::ParseAttributes() | exiting attributes object\n");
	return true;

error:
	return false;
}


bool BFCPMsgFloorRequest::IsValid()
{
	// Must have at least one FloorId attribute.
	if (this->CountFloorIds() < 1) {
		::Error("BFCPMsgFloorRequest::IsValid() | MUST have at least one FloorId attribute\n");
		return false;
	}

	return true;
}


void BFCPMsgFloorRequest::Dump()
{
	::Debug("[BFCPMsgFloorRequest]\n");
	::Debug("- [primitive: FloorRequest, conferenceId: %d, userId: %d, transactionId: %d]\n", this->conferenceId, this->userId, this->transactionId);
	int num_floors = this->floorIds.size();
	for (int i=0; i < num_floors; i++) {
		::Debug("- floodId: %d\n", GetFloorId(i));
	}
	if (this->HasBeneficiaryId()) {
		::Debug("- beneficiaryId: %d\n", GetBeneficiaryId());
	}
	::Debug("[/BFCPMsgFloorRequest]\n");
}


void BFCPMsgFloorRequest::AddFloorId(int floorId)
{
	this->floorIds.push_back(new BFCPAttrFloorId(floorId));
}


void BFCPMsgFloorRequest::SetBeneficiaryId(int beneficiaryId)
{
	// If already set, replace it.
	if (this->beneficiaryId) {
		::Debug("BFCPMsgFloorRequest::SetBeneficiaryId() | attribute 'beneficiaryId' was already set, replacing it\n");
		delete this->beneficiaryId;
	}
	this->beneficiaryId = new BFCPAttrBeneficiaryId(beneficiaryId);
}


void BFCPMsgFloorRequest::SetParticipantProvidedInfo(std::wstring participantProvidedInfo)
{
	// If already set, replace it.
	if (this->participantProvidedInfo) {
		::Debug("BFCPMsgFloorRequest::SetParticipantProvidedInfo() | attribute 'participantProvidedInfo' was already set, replacing it\n");
		delete this->participantProvidedInfo;
	}
	this->participantProvidedInfo = new BFCPAttrParticipantProvidedInfo(participantProvidedInfo);
}


bool BFCPMsgFloorRequest::HasBeneficiaryId()
{
	return (this->beneficiaryId ? true : false);
}


bool BFCPMsgFloorRequest::HasParticipantProvidedInfo()
{
	return (this->participantProvidedInfo ? true : false);
}


int BFCPMsgFloorRequest::GetFloorId(unsigned int index)
{
	if (index >= this->floorIds.size())
		throw BFCPMessage::AttributeNotFound("'floorId' attribute not in range");

	return this->floorIds[index]->GetValue();
}


int BFCPMsgFloorRequest::CountFloorIds()
{
	return this->floorIds.size();
}


int BFCPMsgFloorRequest::GetBeneficiaryId()
{
	if (! this->beneficiaryId)
		throw BFCPMessage::AttributeNotFound("'beneficiaryId' attribute not found");

	return this->beneficiaryId->GetValue();
}


std::wstring BFCPMsgFloorRequest::GetParticipantProvidedInfo()
{
	if (! this->participantProvidedInfo)
		throw BFCPMessage::AttributeNotFound("'participantProvidedInfo' attribute not found");

	return this->participantProvidedInfo->GetValue();
}
