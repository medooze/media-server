#ifndef BFCPATTRIBUTE_H
#define	BFCPATTRIBUTE_H


#include "stringparser.h"
#include <map>
#include <string>
#include <sstream>


class BFCPAttribute
{
/* Static members. */

public:
	enum Name {
		// Single BFCP attributes.
		BeneficiaryId = 1,
		FloorId,
		FloorRequestId,
		Priority,
		RequestStatus,
		ErrorCode,
		ErrorInfo,
		ParticipantProvidedInfo,
		StatusInfo,
		SupportedAttributes,
		SupportedPrimitives,
		UserDisplayName,
		UserUri,
		// Grouped BFCP attributes.
		BeneficiaryInformation,
		FloorRequestInformation,
		RequestedByInformation,
		FloorRequestStatus,
		OverallRequestStatus,
	};

	// For BFCP JSON.
	static std::map<std::wstring, enum Name>	mapJsonStr2Name;
	static std::map<enum Name, std::wstring>	mapName2JsonStr;

public:
	static void Init();

/* Instance members. */

public:
	virtual void Dump() = 0;
};


#endif  /* BFCPATTRIBUTE_H */
