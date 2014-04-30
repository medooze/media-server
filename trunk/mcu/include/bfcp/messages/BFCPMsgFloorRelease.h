#ifndef BFCPMSGFLOORRELEASE_H
#define	BFCPMSGFLOORRELEASE_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/attributes.h"


// 5.3.2.  FloorRelease

//    FloorRelease =   (COMMON-HEADER)
//                     (FLOOR-REQUEST-ID)
//                    *[EXTENSION-ATTRIBUTE]


class BFCPMsgFloorRelease : public BFCPMessage
{
public:
	BFCPMsgFloorRelease(int transactionId, int conferenceId, int userId);
	~BFCPMsgFloorRelease();
	bool ParseAttributes(JSONParser &parser);
	bool IsValid();
	void Dump();

	void SetFloorRequestId(int);
	int GetFloorRequestId();

private:
	// Mandatory attributes.
	BFCPAttrFloorRequestId *floorRequestId;
};


#endif  /* BFCPMSGFLOORRELEASE_H */
