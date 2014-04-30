#ifndef BFCPATTRBENEFICIARYINFORMATION_H
#define	BFCPATTRBENEFICIARYINFORMATION_H


#include "bfcp/BFCPAttribute.h"
#include "bfcp/attributes/BFCPAttrBeneficiaryId.h"


// 5.2.14.  BENEFICIARY-INFORMATION
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |0 0 0 1 1 1 0|M|    Length     |        Beneficiary ID         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  BENEFICIARY-INFORMATION =   (BENEFICIARY-INFORMATION-HEADER)
//                              [USER-DISPLAY-NAME]
//                              [USER-URI]
//                             *[EXTENSION-ATTRIBUTE]


class BFCPAttrBeneficiaryInformation : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrBeneficiaryInformation(int beneficiaryId);
	~BFCPAttrBeneficiaryInformation();
	void Dump();
	void Stringify(std::wstringstream &json_stream);

private:
	// Mandatory attributes.
	BFCPAttrBeneficiaryId *beneficiaryId;
};


#endif
