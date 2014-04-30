#ifndef BFCPATTRFLOORREQUESTINFORMATION_H
#define	BFCPATTRFLOORREQUESTINFORMATION_H


#include "bfcp/BFCPAttribute.h"
#include "bfcp/attributes/BFCPAttrFloorRequestId.h"
#include "bfcp/attributes/BFCPAttrOverallRequestStatus.h"
#include "bfcp/attributes/BFCPAttrFloorRequestStatus.h"
#include "bfcp/attributes/BFCPAttrBeneficiaryInformation.h"
#include "bfcp/attributes/BFCPAttrRequestedByInformation.h"
#include <vector>
#include <string>


// 5.2.15.  FLOOR-REQUEST-INFORMATION
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |0 0 0 1 1 1 1|M|    Length     |       Floor Request ID        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//    FLOOR-REQUEST-INFORMATION =   (FLOOR-REQUEST-INFORMATION-HEADER)
//                                  [OVERALL-REQUEST-STATUS]
//                                1*(FLOOR-REQUEST-STATUS)
//                                  [BENEFICIARY-INFORMATION]
//                                  [REQUESTED-BY-INFORMATION]
//                                  [PRIORITY]
//                                  [PARTICIPANT-PROVIDED-INFO]
//                                 *[EXTENSION-ATTRIBUTE]


class BFCPAttrFloorRequestInformation : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrFloorRequestInformation(int floorRequestId);
	~BFCPAttrFloorRequestInformation();
	void Dump();
	void Stringify(std::wstringstream &json_stream);

	void AddFloorRequestStatus(BFCPAttrFloorRequestStatus *floorRequestStatus);
	void SetOverallRequestStatus(BFCPAttrOverallRequestStatus *overallRequestStatus);
	void SetBeneficiaryInformation(BFCPAttrBeneficiaryInformation *beneficiaryInformation);
	void SetRequestedByInformation(BFCPAttrRequestedByInformation *requestedByInformation);
	void SetDescription(std::wstring& statusInfo);

private:
	// Mandatory attributes.
	BFCPAttrFloorRequestId *floorRequestId;
	std::vector<BFCPAttrFloorRequestStatus *> floorRequestStatuses;
	// Optional attributes.
	BFCPAttrOverallRequestStatus *overallRequestStatus;
	BFCPAttrBeneficiaryInformation *beneficiaryInformation;
	BFCPAttrRequestedByInformation *requestedByInformation;
};


#endif
