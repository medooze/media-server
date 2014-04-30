#include "bfcp/BFCPAttribute.h"


// Initialize static members of the class.
std::map<std::wstring, enum BFCPAttribute::Name>	BFCPAttribute::mapJsonStr2Name;
std::map<enum BFCPAttribute::Name, std::wstring>	BFCPAttribute::mapName2JsonStr;


/* Static methods */

void BFCPAttribute::Init()
{
	mapJsonStr2Name[L"beneficiaryId"] = BeneficiaryId;
	mapJsonStr2Name[L"floorId"] = FloorId;
	mapJsonStr2Name[L"floorRequestId"] = FloorRequestId;
	mapJsonStr2Name[L"priority"] = Priority;
	mapJsonStr2Name[L"requestStatus"] = RequestStatus;
	mapJsonStr2Name[L"errorCode"] = ErrorCode;
	mapJsonStr2Name[L"errorInfo"] = ErrorInfo;
	mapJsonStr2Name[L"participantProvidedInfo"] = ParticipantProvidedInfo;
	mapJsonStr2Name[L"statusInfo"] = StatusInfo;
	mapJsonStr2Name[L"supportedAttributes"] = SupportedAttributes;
	mapJsonStr2Name[L"supportedPrimitives"] = SupportedPrimitives;
	mapJsonStr2Name[L"userDisplayName"] = UserDisplayName;
	mapJsonStr2Name[L"userUri"] = UserUri;
	mapJsonStr2Name[L"beneficiaryInformation"] = BeneficiaryInformation;
	mapJsonStr2Name[L"floorRequestInformation"] = FloorRequestInformation;
	mapJsonStr2Name[L"requestedByInformation"] = RequestedByInformation;
	mapJsonStr2Name[L"floorRequestStatus"] = FloorRequestStatus;
	mapJsonStr2Name[L"overallRequestStatus"] = OverallRequestStatus;

	mapName2JsonStr[BeneficiaryId] = L"beneficiaryId";
	mapName2JsonStr[FloorId] = L"floorId";
	mapName2JsonStr[FloorRequestId] = L"floorRequestId";
	mapName2JsonStr[Priority] = L"priority";
	mapName2JsonStr[RequestStatus] = L"requestStatus";
	mapName2JsonStr[ErrorCode] = L"errorCode";
	mapName2JsonStr[ErrorInfo] = L"errorInfo";
	mapName2JsonStr[ParticipantProvidedInfo] = L"participantProvidedInfo";
	mapName2JsonStr[StatusInfo] = L"statusInfo";
	mapName2JsonStr[SupportedAttributes] = L"supportedAttributes";
	mapName2JsonStr[SupportedPrimitives] = L"supportedPrimitives";
	mapName2JsonStr[UserDisplayName] = L"userDisplayName";
	mapName2JsonStr[UserUri] = L"userUri";
	mapName2JsonStr[BeneficiaryInformation] = L"beneficiaryInformation";
	mapName2JsonStr[FloorRequestInformation] = L"floorRequestInformation";
	mapName2JsonStr[RequestedByInformation] = L"requestedByInformation";
	mapName2JsonStr[FloorRequestStatus] = L"floorRequestStatus";
	mapName2JsonStr[OverallRequestStatus] = L"overallRequestStatus";
}
