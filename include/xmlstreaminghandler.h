#ifndef _XMLSTREAMINGHANDLER_H_
#define _XMLSTREAMINGHANDLER_H_
#include <xmlrpc.h>
#include <list>
#include <map>
#include "config.h"
#include "xmlrpcserver.h"
#include "use.h"


class XmlEvent
{
public:
	virtual ~XmlEvent() = default;
	virtual xmlrpc_value* GetXmlValue(xmlrpc_env *env) = 0;
};

class XmlEventQueue : public Use
{
public:
	XmlEventQueue();
	virtual ~XmlEventQueue();
	void AddEvent(XmlEvent *event);
	void Cancel();
	bool WaitForEvent(DWORD timeout);
	xmlrpc_value* PeekXMLEvent(xmlrpc_env *env);
	void PopEvent();
	
private:
	typedef std::list<XmlEvent*> EventList;

private:
	//The event list
	EventList events;
	bool		cancel;
	pthread_mutex_t	mutex;
	pthread_cond_t  cond;
};

class XmlStreamingHandler :
	public Handler
{
public:
	XmlStreamingHandler();
	~XmlStreamingHandler();
	int CreateEventQueue();
	int AddEvent(DWORD id,XmlEvent *event);
	int DestroyEventQueue(DWORD id);
	virtual int ProcessRequest(TRequestInfo *req,TSession * const ses);

private:
	typedef std::map<DWORD,XmlEventQueue*> EventQueues;

private:
	EventQueues	queues;
	Use		listUse;
	DWORD		maxId;

};

#endif
