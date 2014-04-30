#include "bfcp/messages/BFCPMsgError.h"
#include "log.h"


/* Instance members. */

BFCPMsgError::BFCPMsgError(int transactionId, int conferenceId, int userId) :
		BFCPMessage(BFCPMessage::Error, transactionId, conferenceId, userId),
		errorCode(NULL),
		errorInfo(NULL)
{
}


BFCPMsgError::BFCPMsgError(BFCPMessage *msg, enum BFCPAttrErrorCode::ErrorCode errorCode) :
		BFCPMessage(BFCPMessage::Error, msg->GetTransactionId(), msg->GetConferenceId(), msg->GetUserId()),
		errorCode(new BFCPAttrErrorCode(errorCode)),
		errorInfo(NULL)
{
}


BFCPMsgError::BFCPMsgError(BFCPMessage *msg, enum BFCPAttrErrorCode::ErrorCode errorCode, std::wstring errorInfo) :
		BFCPMessage(BFCPMessage::Error, msg->GetTransactionId(), msg->GetConferenceId(), msg->GetUserId()),
		errorCode(new BFCPAttrErrorCode(errorCode)),
		errorInfo(new BFCPAttrErrorInfo(errorInfo))
{
}


BFCPMsgError::~BFCPMsgError()
{
	::Debug("BFCPMsgError::~BFCPMsgError() | free memory\n");

	if (this->errorCode)
		delete this->errorCode;
	if (this->errorInfo)
		delete this->errorInfo;
}


void BFCPMsgError::Dump()
{
	::Debug("[BFCPMsgError]\n");
	::Debug("- [primitive: Error, conferenceId: %d, userId: %d, transactionId: %d]\n", this->conferenceId, this->userId, this->transactionId);
	::Debug("- errorCode: %ls\n", BFCPAttrErrorCode::mapErrorCode2JsonStr[this->errorCode->GetValue()].c_str());
	if (this->errorInfo) {
		::Debug("- errorInfo: %ls\n", this->errorInfo->GetValue().c_str());
	}
	::Debug("[/BFCPMsgError]\n");
}


std::wstring BFCPMsgError::Stringify()
{
	std::wstringstream json_stream;

	json_stream << L"{";
	StringifyCommonHeader(json_stream);

	// Attributes.
	if (this->errorCode) {
		json_stream << L",";
		json_stream << L"\n\"attributes\": {";
		json_stream << L"\n  \"errorCode\": \"" << this->errorCode->GetString() << L"\"";
		if (this->errorInfo) {
			json_stream << L",\n  \"errorInfo\": \"" << this->errorInfo->GetValue().c_str() << L"\"";
		}
		json_stream << L"\n}";
	}

	// End of root object.
	json_stream << L"\n}";

	return json_stream.str();
}


void BFCPMsgError::SetErrorCode(enum BFCPAttrErrorCode::ErrorCode errorCode)
{
	// If already set, replace it.
	if (this->errorCode) {
		::Debug("BFCPMsgError::SetErrorCode() | attribute 'errorCode' was already set, replacing it\n");
		delete this->errorCode;
	}
	this->errorCode = new BFCPAttrErrorCode(errorCode);
}


bool BFCPMsgError::HasErrorCode()
{
	if (this->errorCode)
		return true;
	else
		return false;
}


enum BFCPAttrErrorCode::ErrorCode BFCPMsgError::GetErrorCode()
{
	if (! this->errorCode)
		throw BFCPMessage::AttributeNotFound("'errorCode' attribute not found");

	return this->errorCode->GetValue();
}
