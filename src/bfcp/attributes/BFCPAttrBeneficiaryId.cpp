#include "bfcp/attributes/BFCPAttrBeneficiaryId.h"
#include "log.h"


/* Instance methods */

BFCPAttrBeneficiaryId::BFCPAttrBeneficiaryId(int value) : value(value)
{
}


void BFCPAttrBeneficiaryId::Dump()
{
	::Debug("[BFCPAttrBeneficiaryId]\n");
	::Debug("- value: %d\n", this->value);
	::Debug("[/BFCPAttrBeneficiaryId]\n");
}


int BFCPAttrBeneficiaryId::GetValue()
{
	return this->value;
}
