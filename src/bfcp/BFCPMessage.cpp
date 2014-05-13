#include "bfcp/BFCPMessage.h"
#include "bfcp/messages.h"
#include "log.h"


// Initialize static members of the class.
std::map<std::wstring, enum BFCPMessage::CommonField>	BFCPMessage::mapJsonStr2CommonField;
std::map<enum BFCPMessage::CommonField, std::wstring>	BFCPMessage::mapCommonField2JsonStr;
std::map<std::wstring, enum BFCPMessage::Primitive>		BFCPMessage::mapJsonStr2Primitive;
std::map<enum BFCPMessage::Primitive, std::wstring>		BFCPMessage::mapPrimitive2JsonStr;


/* Subclasses */

BFCPMessage::AttributeNotFound::AttributeNotFound(const char* description) :
	std::runtime_error(description)
{
}


/* Static methods */

void BFCPMessage::Init()
{
	mapJsonStr2CommonField[L"ver"] = Ver;
	mapJsonStr2CommonField[L"primitive"] = Primitive;
	mapJsonStr2CommonField[L"transactionId"] = TransactionId;
	mapJsonStr2CommonField[L"conferenceId"] = ConferenceId;
	mapJsonStr2CommonField[L"userId"] = UserId;
	mapJsonStr2CommonField[L"attributes"] = Attributes;

	mapCommonField2JsonStr[Ver] = L"ver";
	mapCommonField2JsonStr[Primitive] = L"primitive";
	mapCommonField2JsonStr[TransactionId] = L"transactionId";
	mapCommonField2JsonStr[ConferenceId] = L"conferenceId";
	mapCommonField2JsonStr[UserId] = L"userId";
	mapCommonField2JsonStr[Attributes] = L"attributes";

	mapJsonStr2Primitive[L"FloorRequest"] = FloorRequest;
	mapJsonStr2Primitive[L"FloorRelease"] = FloorRelease;
	mapJsonStr2Primitive[L"FloorRequestQuery"] = FloorRequestQuery;
	mapJsonStr2Primitive[L"FloorRequestStatus"] = FloorRequestStatus;
	mapJsonStr2Primitive[L"UserQuery"] = UserQuery;
	mapJsonStr2Primitive[L"UserStatus"] = UserStatus;
	mapJsonStr2Primitive[L"FloorQuery"] = FloorQuery;
	mapJsonStr2Primitive[L"FloorStatus"] = FloorStatus;
	mapJsonStr2Primitive[L"ChairAction"] = ChairAction;
	mapJsonStr2Primitive[L"ChairActionAck"] = ChairActionAck;
	mapJsonStr2Primitive[L"Hello"] = Hello;
	mapJsonStr2Primitive[L"HelloAck"] = HelloAck;
	mapJsonStr2Primitive[L"Error"] = Error;

	mapPrimitive2JsonStr[FloorRequest] = L"FloorRequest";
	mapPrimitive2JsonStr[FloorRelease] = L"FloorRelease";
	mapPrimitive2JsonStr[FloorRequestQuery] = L"FloorRequestQuery";
	mapPrimitive2JsonStr[FloorRequestStatus] = L"FloorRequestStatus";
	mapPrimitive2JsonStr[UserQuery] = L"UserQuery";
	mapPrimitive2JsonStr[UserStatus] = L"UserStatus";
	mapPrimitive2JsonStr[FloorQuery] = L"FloorQuery";
	mapPrimitive2JsonStr[FloorStatus] = L"FloorStatus";
	mapPrimitive2JsonStr[ChairAction] = L"ChairAction";
	mapPrimitive2JsonStr[ChairActionAck] = L"ChairActionAck";
	mapPrimitive2JsonStr[Hello] = L"Hello";
	mapPrimitive2JsonStr[HelloAck] = L"HelloAck";
	mapPrimitive2JsonStr[Error] = L"Error";
}


BFCPMessage* BFCPMessage::Parse(const std::wstring &json)
{
	JSONParser parser(json);
	BFCPMessage *msg = NULL;
	enum BFCPMessage::Primitive primitive;
	int transactionId;
	int conferenceId;
	int userId;
	wchar_t *attributes_data_pos;
	bool hasVer = false;
	bool hasPrimitive = false;
	bool hasTransactionId = false;
	bool hasConferenceId = false;
	bool hasUserId = false;
	bool hasAttributes = false;

	// Start of root object.
	if (! parser.ParseJSONObjectStart()) {
		::Error("BFCPMessage::Parse() | failed to start root object\n");
		goto error;
	}

	// Empty object?
	if (parser.ParseJSONObjectEnd()) {
		::Error("BFCPMessage::Parse() | empty JSON root object\n");
		goto error;
	}

	// Iterate JSON keys in the root object.
	do {
		// Get key name.
		if (! parser.ParseJSONString()) {
			::Error("BFCPMessage::Parse() | failed to parse JSON key\n");
			goto error;
		}

		enum BFCPMessage::CommonField field = BFCPMessage::mapJsonStr2CommonField[parser.GetValue()];

		if (! parser.ParseDoubleDot()) {
			::Error("BFCPMessage::Parse() | failed to parse ':'\n");
			goto error;
		}

		switch(field) {
			case BFCPMessage::Ver:
				if (! parser.ParseJSONNumber()) {
					::Error("BFCPMessage::Parse() | failed to get 'ver' number\n");
					goto error;
				}

				// MUST be 1.
				if (parser.GetNumberValue() != 1) {
					::Error("BFCPMessage::Parse() | 'ver' != 1\n");
					goto error;
				}

				::Debug("BFCPMessage::Parse() | 'ver' found\n");
				hasVer = true;
				break;

			case BFCPMessage::Primitive:
				if (! parser.ParseJSONString()) {
					::Error("BFCPMessage::Parse() | failed to get 'primitive' value\n");
					goto error;
				}

				// Must be a known "primitive" string.
				primitive = mapJsonStr2Primitive[parser.GetValue()];
				if (! primitive) {
					::Error("BFCPMessage::Parse() | unknown 'primitive'\n");
					goto error;
				}

				::Debug("BFCPMessage::Parse() | 'primitive' found\n");
				hasPrimitive = true;
				break;

			case BFCPMessage::TransactionId:
				if (! parser.ParseJSONNumber()) {
					::Error("BFCPMessage::Parse() | failed to get 'transactionId'\n");
					goto error;
				}

				transactionId = (int)parser.GetNumberValue();
				::Debug("BFCPMessage::Parse() | 'transactionId' found\n");
				hasTransactionId = true;
				break;

			case BFCPMessage::ConferenceId:
				if (! parser.ParseJSONNumber()) {
					::Error("BFCPMessage::Parse() | failed to get 'conferenceId'\n");
					goto error;
				}

				::Debug("BFCPMessage::Parse() | 'conferenceId' found\n");
				conferenceId = (int)parser.GetNumberValue();
				hasConferenceId = true;
				break;

			case BFCPMessage::UserId:
				if (! parser.ParseJSONNumber()) {
					::Error("BFCPMessage::Parse() | failed to get 'userId'\n");
					goto error;
				}

				::Debug("BFCPMessage::Parse() | 'userId' found\n");
				userId = (int)parser.GetNumberValue();
				hasUserId = true;
				break;

			case BFCPMessage::Attributes:
				wchar_t *object_start_pos;
				wchar_t *object_end_pos;

				object_start_pos = parser.Mark();

				// If 'attributes' is found then it MUST be an Object.
				if (! parser.SkipJSONObject()) {
					::Error("BFCPMessage::Parse() | 'attributes' is not an object\n");
					goto error;
				}

				// Mark this position as the end of the 'attributes' object.
				object_end_pos = parser.Mark();

				// Go back to the begining of the object.
				parser.Reset(object_start_pos);

				// Start of object.
				parser.ParseJSONObjectStart();

				// Store the current position as the begining of the 'attributes' data.
				attributes_data_pos = parser.Mark();

				// If an empty object do nothing.
				if (parser.ParseJSONObjectEnd()) {
					::Debug("BFCPMessage::Parse() | 'attributes' object is empty\n");
				} else {
					hasAttributes = true;
				}

				// Reset to the end of the object to continue parsing other keys in the root object.
				parser.Reset(object_end_pos);
				break;

			default:
				::Debug("BFCPMessage::Parse() | skiping unknown key\n");
				parser.SkipJSONValue();
				break;
		}
	} while (parser.ParseComma());

	::Debug("BFCPMessage::Parse() | exiting root object\n");

	// End of root object.
	if (! parser.ParseJSONObjectEnd()) {
		::Error("BFCPMessage::Parse() | failed to end root object\n");
		goto error;
	}

	// Ensure there is nothing else (but spaces).
	parser.SkipJSONSpaces();
	if (! parser.IsEnded()) {
		::Error("BFCPMessage::Parse() | garbage after final '}'\n");
		goto error;
	}

	// Check mandatory common fields.
	if (! (hasVer && hasTransactionId && hasConferenceId && hasUserId)) {
		::Error("BFCPMessage::Parse() | mandatory common field(s) not present\n");
		goto error;
	}

	// Create the corresponding BFCPMessage.

	switch (primitive) {
		case BFCPMessage::FloorRequest:
			msg = (BFCPMessage *)new BFCPMsgFloorRequest(transactionId, conferenceId, userId);
			break;

		case BFCPMessage::FloorRelease:
			msg = (BFCPMessage *)new BFCPMsgFloorRelease(transactionId, conferenceId, userId);
			break;

		case BFCPMessage::FloorQuery:
			msg = (BFCPMessage *)new BFCPMsgFloorQuery(transactionId, conferenceId, userId);
			break;

		case BFCPMessage::Hello:
			msg = (BFCPMessage *)new BFCPMsgHello(transactionId, conferenceId, userId);
			break;

		// TODO: implement more primitives.

		default:
			// Valid but unknown message. Just create a generic BFCPMessage and return it
			// (and don't attempt to parse its attributes).
			msg = new BFCPMessage(primitive, transactionId, conferenceId, userId);
			return msg;
	}

	// Parse attributes.
	if (hasAttributes) {
		parser.Reset(attributes_data_pos);

		if (! msg->ParseAttributes(parser)) {
			::Error("BFCPMessage::Parse() | failed to parse 'attributes' object\n");
			goto error;
		}

		// Ensure mandatory attributes are present among with other checks.
		if (! msg->IsValid()) {
			::Error("BFCPMessage::Parse() | invalid attributes\n");
			goto error;
		}
	}

	// Return parsed message.
	return (BFCPMessage *)msg;


error:
	if (msg)
		delete msg;
	return NULL;
}


/* Instance methods */

BFCPMessage::BFCPMessage(enum BFCPMessage::Primitive primitive, int transactionId, int conferenceId, int userId) :
	primitive(primitive),
	transactionId(transactionId),
	conferenceId(conferenceId),
	userId(userId)
{
}


BFCPMessage::~BFCPMessage()
{
}


bool BFCPMessage::ParseAttributes(JSONParser &parser)
{
	return true;
}


bool BFCPMessage::IsValid()
{
	return true;
}


void BFCPMessage::SetUserId(int userId)
{
	this->userId = userId;
}


enum BFCPMessage::Primitive BFCPMessage::GetPrimitive()
{
	return this->primitive;
}


int BFCPMessage::GetTransactionId()
{
	return this->transactionId;
}


int BFCPMessage::GetConferenceId()
{
	return this->conferenceId;
}


int BFCPMessage::GetUserId()
{
	return this->userId;
}


void BFCPMessage::Dump()
{
	::Debug("[BFCPMessage]\n");
	// NOTE: We can print mapPrimitive2JsonStr[this->primitive].c_str() (which is a wchar_t*)
	// with %ls as long as all its symbols are ASCII (which is true).
	::Debug("- [primitive: %ls, conferenceId: %d, userId: %d, transactionId: %d]\n", mapPrimitive2JsonStr[this->primitive].c_str(), this->conferenceId, this->userId, this->transactionId);
	::Debug("[/BFCPMessage]\n");
}


std::wstring BFCPMessage::Stringify()
{
	std::wstringstream json_stream;

	json_stream << L"{";
	StringifyCommonHeader(json_stream);
	json_stream << L"\n}";

	return json_stream.str();
}


void BFCPMessage::StringifyCommonHeader(std::wstringstream &json_stream)
{
	json_stream << L"\n\"ver\": 1,";
	json_stream << L"\n\"primitive\": \"" << BFCPMessage::mapPrimitive2JsonStr[this->primitive] << L"\",";
	json_stream << L"\n\"transactionId\": " << this->transactionId << L",";
	json_stream << L"\n\"conferenceId\": " << this->conferenceId << L",";
	json_stream << L"\n\"userId\": " << this->userId;
}
