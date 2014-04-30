#ifndef BFCPMSGERROR_H
#define	BFCPMSGERROR_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/attributes.h"


// 5.3.13.  Error

//    Error              =   (COMMON-HEADER)
//                           (ERROR-CODE)
//                           [ERROR-INFO]
//                          *[EXTENSION-ATTRIBUTE]


class BFCPMsgError : public BFCPMessage
{
public:
	BFCPMsgError(int transactionId, int conferenceId, int userId);
	// Constructors to generate responses.
	BFCPMsgError(BFCPMessage *msg, enum BFCPAttrErrorCode::ErrorCode errorCode);
	BFCPMsgError(BFCPMessage *msg, enum BFCPAttrErrorCode::ErrorCode errorCode, std::wstring errorInfo);
	~BFCPMsgError();
	void Dump();
	std::wstring Stringify();

	void SetErrorCode(enum BFCPAttrErrorCode::ErrorCode);
	bool HasErrorCode();
	enum BFCPAttrErrorCode::ErrorCode GetErrorCode();

private:
	// Mandatory attributes.
	BFCPAttrErrorCode *errorCode;
	BFCPAttrErrorInfo *errorInfo;
};


#endif  /* BFCPMSGERROR_H */
