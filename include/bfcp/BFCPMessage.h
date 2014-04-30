#ifndef BFCPMESSAGE_H
#define	BFCPMESSAGE_H


#include "stringparser.h"
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>


class BFCPMessage
{
public:
	// Exception BFCPMessage::AttributeNotFound.
	class AttributeNotFound : public std::runtime_error
	{
	public:
		AttributeNotFound(const char*);
	};

public:
	enum CommonField {
		Ver = 1,
		Primitive,
		TransactionId,
		ConferenceId,
		UserId,
		Attributes
	};

	enum Primitive {
		FloorRequest = 1,
		FloorRelease,
		FloorRequestQuery,
		FloorRequestStatus,
		UserQuery,
		UserStatus,
		FloorQuery,
		FloorStatus,
		ChairAction,
		ChairActionAck,
		Hello,
		HelloAck,
		Error
	};

	// For BFCP JSON.
	static std::map<std::wstring, enum CommonField>		mapJsonStr2CommonField;
	static std::map<enum CommonField, std::wstring>		mapCommonField2JsonStr;
	static std::map<std::wstring, enum Primitive>		mapJsonStr2Primitive;
	static std::map<enum Primitive, std::wstring>		mapPrimitive2JsonStr;

public:
	static void Init();
	// For BFCP JSON.
	static BFCPMessage* Parse(const std::wstring &);

/* Instance members. */

public:
	BFCPMessage(enum BFCPMessage::Primitive primitive, int transactionId, int conferenceId, int userId);
	virtual ~BFCPMessage();
	// Useful for changing the userId of a message.
	void SetUserId(int userId);
	enum BFCPMessage::Primitive GetPrimitive();
	int GetTransactionId();
	int GetConferenceId();
	int GetUserId();
	virtual void Dump();
	virtual std::wstring Stringify();

protected:
	virtual bool IsValid();
	virtual bool ParseAttributes(JSONParser &parser);
	void StringifyCommonHeader(std::wstringstream &json_stream);

protected:
	enum BFCPMessage::Primitive primitive;
	int transactionId;
	int conferenceId;
	int userId;
};


#endif /* BFCPMESSAGE_H */
