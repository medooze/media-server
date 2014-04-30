#include "bfcp/attributes/BFCPAttrStatusInfo.h"
#include "log.h"


/* Instance methods */

BFCPAttrStatusInfo::BFCPAttrStatusInfo(std::wstring value) : value(value)
{
}


void BFCPAttrStatusInfo::Dump()
{
	::Debug("[BFCPAttrStatusInfo]\n");
	::Debug("- value: (not shown)\n");
	::Debug("[/BFCPAttrStatusInfo]\n");
}


std::wstring BFCPAttrStatusInfo::GetValue()
{
	return this->value;
}
