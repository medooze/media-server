#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include <list>
#include "eventstreaminghandler.h"
#include "tools.h"
#include "use.h"

/**************************************
* EventStreamingHandler
*	Constructor
*************************************/
EventStreamingHandler::EventStreamingHandler()
{
}

/**************************************
* EventStreamingHandler
*	Destructor
*************************************/
EventStreamingHandler::~EventStreamingHandler()
{
	//Safe wait
	listUse.WaitUnusedAndLock();

	//Delete all requests
	EventSources::iterator it = sources.begin();

	//Until empty
	while(it!=sources.end())
	{
		//Get request
		Requests* reqs = it->second;

		//Erase from list
		sources.erase(it++);

		//For each one
		for (Requests::iterator itReq = reqs->begin(); itReq!=reqs->end();++itReq)
			//End then
			(*itReq)->Cancel();

		//Delete list
		delete(reqs);
	}

	//Unlock
	listUse.Unlock();
}

/**************************************
* CreateEventQueue
*	Create an event queue and return id
*************************************/
int EventStreamingHandler::CreateEvenSource(const char* source)
{
	Log("-CreateEvenSource [source:%s]\n",source);

	//Get lock on list
	listUse.WaitUnusedAndLock();

	//Create new
	sources[source] = new std::list<StreamRequest*>();

	//Unlock
	listUse.Unlock();

	//OK
	return 1;
}
int EventStreamingHandler::WriteEvent(const char* source,const char* type,const char* msg,va_list params)
{
	char aux[1024];
	//print
	vsprintf(aux, msg, params);
	//Write msg
	return WriteEvent(source,type,aux);
}

int EventStreamingHandler::WriteEvent(const char* source,const Event &event)
{
	return WriteEvent(source,event.getType(),event.getData());
}

int EventStreamingHandler::WriteEvent(const char* source,const char* type,const char *msg)
{
	//We are using the list
	listUse.IncUse();

	//Find queue
	EventSources::iterator it = sources.find(source);

	//If not found
	if (it==sources.end())
	{
		//Not using it anymore
		listUse.DecUse();
		//Mandamos error
		return Error("-WriteEvent source not found [source:%s]\n",source);
	}

	//Get list of request for that source
	Requests *reqs = it->second;

	//Get first
	Requests::iterator itReq = reqs->begin();
	
	//For each one
	while(itReq!=reqs->end())
	{
		//Write it
		if (!(*itReq)->WriteEvent(type,msg))
			//remove if error
			reqs->erase(itReq++);
		else
			//Next one
			++itReq;
	}

	//Not using it anymore the list
	listUse.DecUse();

	//Done
	return 1;
}

int EventStreamingHandler::DestroyEvenSource(const char* source)
{
	Log("-DestroyEvenSource [source:%s]\n",source);
	
	//Get lock on list
	listUse.WaitUnusedAndLock();

	//Find it
	EventSources::iterator it = sources.find(source);

	//If not found
	if (it==sources.end())
	{
		//Unlock
		listUse.Unlock();
		//Exit
		return 0;

	}
	//Get request
	Requests* reqs = it->second;

	//Erase from list
	sources.erase(it);

	//Unlock
	listUse.Unlock();

	//For each one
	for (Requests::iterator itReq = reqs->begin(); itReq!=reqs->end();++itReq)
		//End then
		(*itReq)->Cancel();

	//Delete list
	delete(reqs);
	
	//Done
	return 1;
}

/**************************************
* ProcessRequest
*	Procesa una peticion
*************************************/
int EventStreamingHandler::ProcessRequest(TRequestInfo *req,TSession * const ses)
{
	Log("-ProcessEventRequest [uri:%s]\n",req->uri);

	//Block signals
	blocksignals();

	//Allow CORS
	ResponseAddField(ses,"Access-Control-Allow-Origin","*");

	//Get las
	char *i = strrchr((char*)req->uri,'/');

	//Check if it was not found
	if (!i)
		//Mandamos error
		return XmlRpcServer::SendError(ses, 404, "Not found");

	//Get source
	char *source = i+1;

	//We are using the list
	listUse.IncUse();

	//Find queue
	EventSources::iterator it = sources.find(source);

	//If not found
	if (it==sources.end())
	{
		//Not using it anymore
		listUse.DecUse();
		//Error
		Error("-Source not found [source=%s]\n",source);
		//Mandamos error
		return XmlRpcServer::SendError(ses, 404, "Not found");
	}

	//Create new request
	StreamRequest *request = new StreamRequest(ses);
	
	//Add to queu
	it->second->push_back(request);

	//Not using it anymore
	listUse.DecUse();

	//Set the content type
	ResponseContentType(ses, (char*)"text/event-stream");

	//Chunked output
	ResponseChunked(ses);

	//Send OK
	ResponseStatus(ses,200);

	//Start writing
	ResponseWriteStart(ses);

	//Wait fot end
	request->WaitSignal(0);

	//End it
	ResponseWriteEnd(ses);

	return 1;
}

