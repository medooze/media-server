#ifndef BFCPATTROVERALLREQUESTSTATUS
#define	BFCPATTROVERALLREQUESTSTATUS


#include "bfcp/BFCPAttribute.h"
#include "bfcp/attributes/BFCPAttrFloorRequestId.h"
#include "bfcp/attributes/BFCPAttrRequestStatus.h"
#include "bfcp/attributes/BFCPAttrStatusInfo.h"


// 5.2.18.  OVERALL-REQUEST-STATUS
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |0 0 1 0 0 1 0|M|    Length     |       Floor Request ID        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//    OVERALL-REQUEST-STATUS   =   (OVERALL-REQUEST-STATUS-HEADER)
//                                 [REQUEST-STATUS]
//                                 [STATUS-INFO]
//                                *[EXTENSION-ATTRIBUTE]


class BFCPAttrOverallRequestStatus : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrOverallRequestStatus(int floorRequestId);
	~BFCPAttrOverallRequestStatus();
	void Dump();
	void Stringify(std::wstringstream &json_stream);

	void SetRequestStatus(BFCPAttrRequestStatus *requestStatus);
	void SetStatusInfo(BFCPAttrStatusInfo *statusInfo);

private:
	// Mandatory attributes.
	BFCPAttrFloorRequestId *floorRequestId;
	// Optional attributes.
	BFCPAttrRequestStatus *requestStatus;
	BFCPAttrStatusInfo *statusInfo;
};


#endif
