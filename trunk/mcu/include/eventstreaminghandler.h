#ifndef _EVENTSTREAMINGHANDLER_H_
#define _EVENTSTREAMINGHANDLER_H_
#include <list>
#include <map>
#include <stdarg.h>
#include "config.h"
#include "log.h"
#include "xmlrpcserver.h"
#include "use.h"
#include "wait.h"
#include "amf.h"

class Event
{
public:
	virtual const char* getType() const = 0;
	virtual const char* getData() const = 0;
};

class StringEvent : public Event
{
public:
	StringEvent(const char* type,const char* str)
	{
		this->type = type;
		this->str = str;
	}
	StringEvent(const char* str)
	{
		this->type = NULL;
		this->str = str;
	}

	virtual const char* getData() const
	{
		return str;
	}
	virtual const char* getType() const
	{
		return str;
	}
private:
	const char* type;
	const char* str;
};

class StreamRequest : public Wait
{
public:
	StreamRequest(TSession * sess)
	{
		this->sess = sess;
	}

	int WriteEvent(const char *msg)
	{
		return WriteEvent(NULL,msg);
	}

	int WriteEvent(const char* type,const char *msg)
	{
		char aux[1024];

		if (type!=NULL)
			//Serialize
			sprintf(aux,"event:%s\ndata:%s\n\n",type,msg);
		else
			//Serialize
			sprintf(aux,"data:%s\n\n",msg);
		//Send it
		if (!ResponseWriteBody(sess,aux,strlen(aux)))
			//Error
			return 0;
		//Exit
		return 1;
	}

private:
	TSession*	sess;
};

struct strcomp
{
    bool operator() (const char *a,const char* b)
    {
        return strcmp(a,b) < 0;
    }
};

class EventStreamingHandler :
	public Handler
{
public:
	EventStreamingHandler();
	~EventStreamingHandler();
	int CreateEvenSource(const char* source);
	int WriteEvent(const char* source,const Event &event);
	int WriteEvent(const char* source,const char* type,const char* msg);
	int WriteEvent(const char* source,const char* type,const char* msg,va_list params);
	int DestroyEvenSource(const char* source);
	
	virtual int ProcessRequest(TRequestInfo *req,TSession * const ses);

private:
	typedef std::list<StreamRequest*> Requests;
	typedef std::map<const char*,Requests*, strcomp> EventSources;

private:
	EventSources	sources;
	Use		listUse;
};

class EvenSource
{
public:
	static EventStreamingHandler& getInstance()
        {
            static EventStreamingHandler   ev;
            return ev;
        }

	EvenSource()
	{
		//alocate mem
		source = (char*)malloc(sizeof(void*)*2+4);
		//Create event source
		sprintf(source,"%p",this);
		//Create source
		getInstance().CreateEvenSource(source);
	}

	EvenSource(const char* str)
	{
		//Duplicate
		source = strdup(str);
		//Create source
		getInstance().CreateEvenSource(source);
	}

	EvenSource(const std::wstring &str)
	{
		//Convert to utf
		UTF8Parser parser(str);
		//Get size
		DWORD size = parser.GetUTF8Size();
		//alocate mem
		source = (char*)malloc(size+1);
		//Serialize
		parser.Serialize((BYTE*)source,size);
		//End it
		source[size] = 0;
		//Create source
		getInstance().CreateEvenSource(source);
	}
	
	~EvenSource()
	{
		//Destroy source
		getInstance().DestroyEvenSource(source);
		//Free memory
		free(source);
	}

	void SendEvent(const char* type,const char* msg,...)
	{
		va_list params;
		//Initialize parameeter list
		va_start(params, msg);
		//Write event
		getInstance().WriteEvent(source,type,msg,params);
		//End parameter list
		va_end(params);
	}
private:
	char* source;
};

#endif
