#ifndef BFCPATTRFLOORREQUESTSTATUS
#define	BFCPATTRFLOORREQUESTSTATUS


#include "bfcp/BFCPAttribute.h"
#include "bfcp/attributes/BFCPAttrFloorId.h"
#include "bfcp/attributes/BFCPAttrRequestStatus.h"


// 5.2.17.  FLOOR-REQUEST-STATUS
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |0 0 1 0 0 0 1|M|    Length     |           Floor ID            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  FLOOR-REQUEST-STATUS     =   (FLOOR-REQUEST-STATUS-HEADER)
//                               [REQUEST-STATUS]
//                               [STATUS-INFO]
//                               *[EXTENSION-ATTRIBUTE]


class BFCPAttrFloorRequestStatus : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrFloorRequestStatus(int floorId);
	~BFCPAttrFloorRequestStatus();
	void Dump();
	void Stringify(std::wstringstream &json_stream);

	void SetRequestStatus(BFCPAttrRequestStatus *requestStatus);

private:
	// Mandatory attributes.
	BFCPAttrFloorId *floorId;
	// Optional attributes.
	BFCPAttrRequestStatus *requestStatus;
};


#endif
