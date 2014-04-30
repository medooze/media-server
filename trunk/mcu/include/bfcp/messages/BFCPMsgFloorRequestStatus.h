#ifndef BFCPMSGFLOORREQUESTSTATUS_H
#define	BFCPMSGFLOORREQUESTSTATUS_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/attributes.h"
#include <string>


// 5.3.4.  FloorRequestStatus
//
//    FloorRequestStatus =   (COMMON-HEADER)
//                           (FLOOR-REQUEST-INFORMATION)
//                          *[EXTENSION-ATTRIBUTE]


class BFCPMsgFloorRequestStatus :  public BFCPMessage
{
public:
	BFCPMsgFloorRequestStatus(int transactionId, int conferenceId, int userId);
	~BFCPMsgFloorRequestStatus();
	void Dump();
	std::wstring Stringify();

	void SetFloorRequestInformation(BFCPAttrFloorRequestInformation *floorRequestInformation);
	void SetDescription(std::wstring& statusInfo);

private:
	// Mandatory attributes.
	BFCPAttrFloorRequestInformation *floorRequestInformation;
};


#endif  /* BFCPMSGFLOORREQUESTSTATUS_H */
