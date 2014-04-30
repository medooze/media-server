#ifndef BFCPATTRFLOORID_H
#define	BFCPATTRFLOORID_H


#include "bfcp/BFCPAttribute.h"


class BFCPAttrFloorId : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrFloorId(int);
	void Dump();

	int GetValue();

private:
	int value;
};


#endif
