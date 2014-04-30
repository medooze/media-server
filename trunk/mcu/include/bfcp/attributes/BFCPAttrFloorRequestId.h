#ifndef BFCPATTRFLOORREQUESTID_H
#define	BFCPATTRFLOORREQUESTID_H


#include "bfcp/BFCPAttribute.h"


class BFCPAttrFloorRequestId : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrFloorRequestId(int);
	void Dump();

	int GetValue();

private:
	int value;
};


#endif
