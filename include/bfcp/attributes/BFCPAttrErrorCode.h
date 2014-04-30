#ifndef BFCPATTRERRORCODE_H
#define	BFCPATTRERRORCODE_H


#include "bfcp/BFCPAttribute.h"
#include <string>


class BFCPAttrErrorCode : public BFCPAttribute
{
/* Static members. */

public:
	enum ErrorCode {
		ConferenceDoesNotExist = 1,
		UserDoesNotExist,
		UnknownPrimitive,
		UnknownMandatoryAttribute,
		UnauthorizedOperation,
		InvalidFloorId,
		FloorRequestIdDoesNotExist,
		FloorRequestMaxNumberReached,
		UseTls
	};

	// For BFCP JSON.
	static std::map<enum ErrorCode, std::wstring>	mapErrorCode2JsonStr;

public:
	static void Init();

/* Instance members. */

public:
	BFCPAttrErrorCode(enum ErrorCode errorCode);
	void Dump();

	enum ErrorCode GetValue();
	std::wstring GetString();

private:
	enum ErrorCode value;
};


#endif
