#ifndef _STATUSHANDLER_H_
#define _STATUSHANDLER_H_
#include "xmlrpcserver.h"

class StatusHandler :
	public Handler
{
public:
	virtual int ProcessRequest(TRequestInfo *req,TSession * const ses);
};


#endif
