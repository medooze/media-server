#include "bfcp/attributes/BFCPAttrErrorInfo.h"
#include "log.h"


/* Instance methods */

BFCPAttrErrorInfo::BFCPAttrErrorInfo(std::wstring value) : value(value)
{
}


void BFCPAttrErrorInfo::Dump()
{
	::Debug("[BFCPAttrErrorInfo]\n");
	::Debug("- value: (not shown)\n");
	::Debug("[/BFCPAttrErrorInfo]\n");
}


std::wstring BFCPAttrErrorInfo::GetValue()
{
	return this->value;
}
