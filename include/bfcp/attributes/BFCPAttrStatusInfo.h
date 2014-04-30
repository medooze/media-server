#ifndef BFCPATTRSTATUSINFO_H
#define	BFCPATTRSTATUSINFO_H


#include "bfcp/BFCPAttribute.h"
#include <string>


class BFCPAttrStatusInfo : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrStatusInfo(std::wstring value);
	void Dump();

	std::wstring GetValue();

private:
	std::wstring value;
};


#endif
