#include "bfcp/messages/BFCPMsgHello.h"
#include "log.h"


/* Instance members. */

BFCPMsgHello::BFCPMsgHello(int transactionId, int conferenceId, int userId) :
		BFCPMessage(BFCPMessage::Hello, transactionId, conferenceId, userId)
{
}


BFCPMsgHello::~BFCPMsgHello()
{
	::Debug("BFCPMsgHello::~BFCPMsgHello() | free memory\n");
}

bool BFCPMsgHello::ParseAttributes(JSONParser &parser)
{
	return true;
}


bool BFCPMsgHello::IsValid()
{
	// Always valid.
	return true;
}


void BFCPMsgHello::Dump()
{
	::Debug("[BFCPMsgHello]\n");
	::Debug("- [primitive: Hello, conferenceId: %d, userId: %d, transactionId: %d]\n", this->conferenceId, this->userId, this->transactionId);
	::Debug("[/BFCPMsgHello]\n");
}
