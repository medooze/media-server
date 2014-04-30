#ifndef BFCPATTRBENEFICIARYIDA_H
#define	BFCPATTRBENEFICIARYIDA_H


#include "bfcp/BFCPAttribute.h"


class BFCPAttrBeneficiaryId : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrBeneficiaryId(int);
	void Dump();

	int GetValue();

private:
	int value;
};


#endif
