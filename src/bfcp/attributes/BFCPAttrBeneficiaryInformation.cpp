#include "bfcp/attributes/BFCPAttrBeneficiaryInformation.h"
#include "log.h"


/* Instance methods */

BFCPAttrBeneficiaryInformation::BFCPAttrBeneficiaryInformation(int beneficiaryId) :
	beneficiaryId(new BFCPAttrBeneficiaryId(beneficiaryId))
{
}


BFCPAttrBeneficiaryInformation::~BFCPAttrBeneficiaryInformation()
{
	::Debug("BFCPAttrBeneficiaryInformation::~BFCPAttrBeneficiaryInformation() | free memory\n");

	delete this->beneficiaryId;
}


void BFCPAttrBeneficiaryInformation::Dump()
{
	::Debug("[BFCPAttrBeneficiaryInformation]\n");
	::Debug("- beneficiaryId: %d\n", this->beneficiaryId->GetValue());
	::Debug("[/BFCPAttrBeneficiaryInformation]\n");
}


void BFCPAttrBeneficiaryInformation::Stringify(std::wstringstream &json_stream)
{
	json_stream << L"{";
	json_stream << L"\n  \"beneficiaryId\": " << this->beneficiaryId->GetValue();
	json_stream << L"\n}";
}
