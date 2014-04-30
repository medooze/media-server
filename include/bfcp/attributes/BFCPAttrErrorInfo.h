#ifndef BFCPATTRERRORINFO_H
#define	BFCPATTRERRORINFO_H


#include "bfcp/BFCPAttribute.h"
#include <string>


class BFCPAttrErrorInfo : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrErrorInfo(std::wstring value);
	void Dump();

	std::wstring GetValue();

private:
	std::wstring value;
};


#endif
