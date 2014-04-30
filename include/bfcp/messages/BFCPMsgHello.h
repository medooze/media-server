#ifndef BFCPMSGHELLO_H
#define	BFCPMSGHELLO_H


#include "bfcp/BFCPMessage.h"
#include "bfcp/attributes.h"


// 5.3.11.  Hello

//    Hello         =  (COMMON-HEADER)
//                    *[EXTENSION-ATTRIBUTE]


class BFCPMsgHello : public BFCPMessage
{
public:
	BFCPMsgHello(int transactionId, int conferenceId, int userId);
	~BFCPMsgHello();
	bool ParseAttributes(JSONParser &parser);
	bool IsValid();
	void Dump();

private:
};


#endif  /* BFCPMSGHELLO_H */
