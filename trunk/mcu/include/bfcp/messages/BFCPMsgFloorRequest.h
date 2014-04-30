#ifndef BFCPMSGFLOORREQUEST_H
#define	BFCPMSGFLOORREQUEST_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/attributes.h"
#include <vector>


// 5.3.1.  FloorRequest
//
//    FloorRequest =   (COMMON-HEADER)
//                   1*(FLOOR-ID)
//                     [BENEFICIARY-ID]
//                     [PARTICIPANT-PROVIDED-INFO]
//                     [PRIORITY]
//                    *[EXTENSION-ATTRIBUTE]


class BFCPMsgFloorRequest : public BFCPMessage
{
public:
	BFCPMsgFloorRequest(int transactionId, int conferenceId, int userId);
	~BFCPMsgFloorRequest();
	bool ParseAttributes(JSONParser &parser);
	bool IsValid();
	void Dump();

	void AddFloorId(int);
	void SetBeneficiaryId(int);
	void SetParticipantProvidedInfo(std::wstring);
	bool HasBeneficiaryId();
	bool HasParticipantProvidedInfo();
	int GetFloorId(unsigned int);
	int CountFloorIds();
	int GetBeneficiaryId();
	std::wstring GetParticipantProvidedInfo();

private:
	// Mandatory attributes.
	std::vector<BFCPAttrFloorId *> floorIds;
	// Optional attributes.
	BFCPAttrBeneficiaryId *beneficiaryId;
	BFCPAttrParticipantProvidedInfo *participantProvidedInfo;
};


#endif  /* BFCPMSGFLOORREQUEST_H */
