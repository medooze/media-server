#ifndef _XMLRPCSERVER_H_
#define _XMLRPCSERVER_H_
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <xmlrpc.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/server_abyss.h>
#include <string>
#include <map>
#include "config.h"


class Handler
{
public:
	virtual int ProcessRequest(TRequestInfo *req,TSession * const ses)=0;
};

class XmlRpcServer
{
public:
	XmlRpcServer(int port, const char* iface = NULL);
	~XmlRpcServer();
	int AddHandler(std::string base,Handler* hnd);

	int Start(bool async);
	int Stop();

	static void RequestHandler(void *par,TSession *ses, abyss_bool *ret);
	static int GetBody(TSession *r,char *body,DWORD bodyLen);
	static int SendResponse(TSession *r, short code, const char *msg, int length);
	static int SendError(TSession * r, short code);
	static int SendError(TSession * r, short code, const char *msg);

protected:
	int DispatchRequest(TSession *ses);
	int Run();
private:
	static void * run(void *par);
private:
	typedef std::map<std::string,Handler *> LstHandlers;
	int running;
	in_addr_t iface;
	int port;
	TServer srv;
	LstHandlers lstHandlers;

};

#endif
