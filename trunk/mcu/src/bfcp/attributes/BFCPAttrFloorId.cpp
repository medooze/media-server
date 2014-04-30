#include "bfcp/attributes/BFCPAttrFloorId.h"
#include "log.h"


/* Instance methods */

BFCPAttrFloorId::BFCPAttrFloorId(int value) : value(value)
{
}


void BFCPAttrFloorId::Dump()
{
	::Debug("[BFCPAttrFloorId]\n");
	::Debug("- value: %d\n", this->value);
	::Debug("[/BFCPAttrFloorId]\n");
}


int BFCPAttrFloorId::GetValue()
{
	return this->value;
}
