#ifndef BFCPATTRPARTICIPANTPROVIDEDINFO_H
#define	BFCPATTRPARTICIPANTPROVIDEDINFO_H


#include "bfcp/BFCPAttribute.h"
#include <string>


class BFCPAttrParticipantProvidedInfo : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrParticipantProvidedInfo(std::wstring);
	void Dump();

	std::wstring GetValue();

private:
	std::wstring value;
};


#endif
