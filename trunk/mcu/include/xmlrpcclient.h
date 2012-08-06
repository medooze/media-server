#ifndef _XMLRPCCLIENT_H_
#define _XMLRPCCLIENT_H_
#include <xmlrpc.h>

class XmlRpcClient
{
public:
	static int Init();
	static int End();
	int MakeCall(const char *uri,const char *method,xmlrpc_value *params,xmlrpc_value **result);
};
#endif
