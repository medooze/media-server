#include "bfcp/attributes/BFCPAttrRequestedByInformation.h"
#include "log.h"


/* Instance methods */

BFCPAttrRequestedByInformation::BFCPAttrRequestedByInformation(int requestedById) :
	requestedById(requestedById)
{
}


void BFCPAttrRequestedByInformation::Dump()
{
	::Debug("[BFCPAttrRequestedByInformation]\n");
	::Debug("- requestedById: %d\n", this->requestedById);
	::Debug("[/BFCPAttrRequestedByInformation]\n");
}


void BFCPAttrRequestedByInformation::Stringify(std::wstringstream &json_stream)
{
	json_stream << L"{";
	json_stream << L"\n  \"requestedById\": " << this->requestedById;
	json_stream << L"\n}";
}
