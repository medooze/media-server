#include "bfcp/attributes/BFCPAttrParticipantProvidedInfo.h"
#include "log.h"


/* Instance methods */

BFCPAttrParticipantProvidedInfo::BFCPAttrParticipantProvidedInfo(std::wstring value) : value(value)
{
}


void BFCPAttrParticipantProvidedInfo::Dump()
{
	::Debug("[BFCPAttrParticipantProvidedInfo]\n");
	// NOTE: Cannot print a UTF8 wide string which can have multibyte symbols.
	//::Debug("- value: %ls\n", this->value);
	::Debug("- value: (not shown)\n");
	::Debug("[/BFCPAttrParticipantProvidedInfo]\n");
}


std::wstring BFCPAttrParticipantProvidedInfo::GetValue()
{
	return this->value;
}
