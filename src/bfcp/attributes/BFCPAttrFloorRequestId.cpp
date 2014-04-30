#include "bfcp/attributes/BFCPAttrFloorRequestId.h"
#include "log.h"


/* Instance methods */

BFCPAttrFloorRequestId::BFCPAttrFloorRequestId(int value) : value(value)
{
}


void BFCPAttrFloorRequestId::Dump()
{
	::Debug("[BFCPAttrFloorRequestId]\n");
	::Debug("- value: %d\n", this->value);
	::Debug("[/BFCPAttrFloorRequestId]\n");
}


int BFCPAttrFloorRequestId::GetValue()
{
	return this->value;
}
