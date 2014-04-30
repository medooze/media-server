#include "bfcp/attributes/BFCPAttrErrorCode.h"
#include "log.h"


// Initialize static members of the class.
std::map<enum BFCPAttrErrorCode::ErrorCode, std::wstring>	BFCPAttrErrorCode::mapErrorCode2JsonStr;


/* Static methods */

void BFCPAttrErrorCode::Init()
{
	mapErrorCode2JsonStr[ConferenceDoesNotExist] = L"ConferenceDoesNotExist";
	mapErrorCode2JsonStr[UserDoesNotExist] = L"UserDoesNotExist";
	mapErrorCode2JsonStr[UnknownPrimitive] = L"UnknownPrimitive";
	mapErrorCode2JsonStr[UnknownMandatoryAttribute] = L"UnknownMandatoryAttribute";
	mapErrorCode2JsonStr[UnauthorizedOperation] = L"UnauthorizedOperation";
	mapErrorCode2JsonStr[InvalidFloorId] = L"InvalidFloorId";
	mapErrorCode2JsonStr[FloorRequestIdDoesNotExist] = L"FloorRequestIdDoesNotExist";
	mapErrorCode2JsonStr[FloorRequestMaxNumberReached] = L"FloorRequestMaxNumberReached";
	mapErrorCode2JsonStr[UseTls] = L"UseTls";
}


/* Instance methods */

BFCPAttrErrorCode::BFCPAttrErrorCode(enum BFCPAttrErrorCode::ErrorCode value) : value(value)
{
}


void BFCPAttrErrorCode::Dump()
{
	::Debug("[BFCPAttrErrorCode]\n");
	::Debug("- value: %ls\n", GetString().c_str());
	::Debug("[/BFCPAttrErrorCode]\n");
}


enum BFCPAttrErrorCode::ErrorCode BFCPAttrErrorCode::GetValue()
{
	return this->value;
}


std::wstring BFCPAttrErrorCode::GetString()
{
	return BFCPAttrErrorCode::mapErrorCode2JsonStr[this->value];
}
