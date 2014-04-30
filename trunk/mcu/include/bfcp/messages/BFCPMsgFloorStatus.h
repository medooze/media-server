#ifndef BFCPMSGFLOORSTATUS_H
#define	BFCPMSGFLOORSTATUS_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/attributes.h"
#include <vector>


// 5.3.8.  FloorStatus
//
//    FloorStatus        =     (COMMON-HEADER)
//                           *1(FLOOR-ID)
//                            *[FLOOR-REQUEST-INFORMATION]
//                            *[EXTENSION-ATTRIBUTE]


/*
 * NOTE:
 * The above is not entirely true. According to the RFC text, a FloorStatus can just have
 * zero or one FLOOR-ID.
 */


// TODO: WRONG: must have a vector of BFCPAttrFloorRequestInformation*.


class BFCPMsgFloorStatus :  public BFCPMessage
{
public:
	BFCPMsgFloorStatus(int transactionId, int conferenceId, int userId);
	~BFCPMsgFloorStatus();
	void Dump();
	std::wstring Stringify();

	void SetFloorId(int);
	void AddFloorRequestInformation(BFCPAttrFloorRequestInformation* floorRequestInformation);
	int GetFloorId();

private:
	// Optional attributes.
	BFCPAttrFloorId* floorId;
	std::vector<BFCPAttrFloorRequestInformation*> floorRequestInformations;
};


#endif
