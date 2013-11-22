#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include <list>
#include "xmlstreaminghandler.h"
#include "tools.h"
#include "use.h"


XmlEventQueue::XmlEventQueue()
{
	cancel = false;
	//Create mutex
	pthread_mutex_init(&mutex,NULL);
	//Create condition
	pthread_cond_init(&cond,0);

}
XmlEventQueue::~XmlEventQueue()
{
	//Lock
	pthread_mutex_lock(&mutex);

	//While events
	while (!events.empty())
	{
		//delet first
		delete(events.front());
		//And remove it froom server
		events.pop_front();
	}

	//UnLock
	pthread_mutex_unlock(&mutex);

	//Destroy event
	pthread_mutex_destroy(&mutex);
	//Destroy condition
	pthread_cond_destroy(&cond);
}

void XmlEventQueue::AddEvent(XmlEvent *event)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Add event
	events.push_back(event);

	//Unlock
	pthread_mutex_unlock(&mutex);

	//Signal
	pthread_cond_signal(&cond);
}

void XmlEventQueue::Cancel()
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Canceled
	cancel = true;

	//Unlock
	pthread_mutex_unlock(&mutex);

	//Signal condition
	pthread_cond_signal(&cond);
}

bool XmlEventQueue::WaitForEvent(DWORD timeout)
{
	timespec ts;

	//Lock
	pthread_mutex_lock(&mutex);

	//if we are cancel
	if (cancel)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//canceled
		return false;
	}

	//If there are no events in the queue
	if (events.empty())
	{
		//Calculate timeout
		calcTimout(&ts,timeout);

		//Esperamos la condicion
		pthread_cond_timedwait(&cond,&mutex,&ts);
	}

	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//There are events in the queue
	return true;
}

xmlrpc_value* XmlEventQueue::PeekXMLEvent(xmlrpc_env *env)
{
	xmlrpc_value* val = NULL;

	//Lock
	pthread_mutex_lock(&mutex);

	//Get event
	if (!events.empty())
		//Retreive firs
		val = events.front()->GetXmlValue(env);

	//UnLock
	pthread_mutex_unlock(&mutex);

	return val;
}

void XmlEventQueue::PopEvent()
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Get event
	if (!events.empty())
	{
		//delet first
		delete(events.front());
		//And remove it froom server
		events.pop_front();
	}

	//UnLock
	pthread_mutex_unlock(&mutex);
}

/**************************************
* XmlStreamingHandler
*	Constructor
*************************************/
XmlStreamingHandler::XmlStreamingHandler()
{
	timeval tv;
	
	//Get secs
	gettimeofday(&tv,NULL);

	//El id inicial
	maxId = (tv.tv_sec & 0x7FFF) << 16;

}

/**************************************
* XmlStreamingHandler
*	Destructor
*************************************/
XmlStreamingHandler::~XmlStreamingHandler()
{
	//Safe wait
	listUse.WaitUnusedAndLock();
	//For each queue
	for (EventQueues::iterator it = queues.begin(); it!=queues.end(); ++it)
		//Delete queue
		delete(it->second);
	//Empty
	queues.clear();
	//Unlock
	listUse.Unlock();
}

/**************************************
* CreateEventQueue
*	Create an event queue and return id
*************************************/
int XmlStreamingHandler::CreateEventQueue()
{
	//Create queue
	XmlEventQueue *queue = new XmlEventQueue();

	//Get lock on list
	listUse.WaitUnusedAndLock();

	//Inc id and get id
	DWORD id = maxId++;

	//Appand
	queues[id] = queue;

	//Unlock
	listUse.Unlock();

	//Return queue id
	return id;
}

int XmlStreamingHandler::AddEvent(DWORD id,XmlEvent *event)
{
	//We are using the list
	listUse.IncUse();

	//Find queue
	EventQueues::iterator it = queues.find(id);

	//If not found
	if (it==queues.end())
	{
		//Not using it anymore
		listUse.DecUse();
		//Mandamos error
		return 0;
	}

	//Get queue
	XmlEventQueue *queue = it->second;

	//Inc queue usage
	queue->IncUse();

	//Not using it anymore the list
	listUse.DecUse();

	//Add the event to the queue
	queue->AddEvent(event);

	//Stop using queue
	queue->DecUse();

	//Done
	return 1;
}



int XmlStreamingHandler::DestroyEventQueue(DWORD id)
{
	Log("-Destroy event queue [id:%d]\n",id);
	
	//Get lock
	listUse.WaitUnusedAndLock();

	//Find queue
	EventQueues::iterator it = queues.find(id);

	//If not found
	if (it==queues.end())
	{
		//Not using it anymore
		listUse.Unlock();
		//Mandamos error
		return Error("Event queue not found\n");
	}

	//Get queue
	XmlEventQueue *queue = it->second;

	//Remove it from the queue
	queues.erase(it);

	//Unlock queue
	listUse.Unlock();

	//Cancel any pending activity
	queue->Cancel();

	//Wait until it is not used anymore
	queue->WaitUnusedAndLock();

	//Delete queu
	delete(queue);
	
	//Done
	return 1;
}

/**************************************
* ProcessRequest
*	Procesa una peticion
*************************************/
int XmlStreamingHandler::ProcessRequest(TRequestInfo *req,TSession * const ses)
{
	XmlEvent* event;
	xmlrpc_env env;
	timeval tv;

	Log(">ProcessRequest [uri:%s]\n",req->uri);

	//Block signals
	blocksignals();

	//Init timer
	getUpdDifTime(&tv);

	//Get las
	char *i = strrchr((char*)req->uri,'/');

	//Check if it was not found
	if (!i)
		//Mandamos error
		return XmlRpcServer::SendError(ses, 404, "Not found");

	//Get queue id
	DWORD id = atoi(i+1);

	//We are using the list
	listUse.IncUse();

	//Find queue
	EventQueues::iterator it = queues.find(id);

	//If not found
	if (it==queues.end())
	{
		//Not using it anymore
		listUse.DecUse();
		//Mandamos error
		return XmlRpcServer::SendError(ses, 404, "Not found");
	}

	//Get queue
	XmlEventQueue *queue = it->second;

	//Inc queue usage
	queue->IncUse();

	//Not using it anymore
	listUse.DecUse();

	//Creamos un enviroment
	xmlrpc_env_init(&env);

	//Set the content type
	ResponseContentType(ses, (char*)"text/xml; charset=\"utf-8\"");

	//Chunked output
	ResponseChunked(ses);

	//Send OK
	ResponseStatus(ses,200);

	//Start writing
	ResponseWriteStart(ses);

	//Get next events
	while(queue->WaitForEvent(30000))
	{
		//Get xml rpc object value
		xmlrpc_value *val = queue->PeekXMLEvent(&env);

		//If no event
		if (!val)
		{
			//Send keep alive;
			if (!ResponseWriteBody(ses,"\r\n",2))
				//Close on error
				break;
			//Wait again
			continue;
		}

		//Create mem block
		xmlrpc_mem_block *output = xmlrpc_mem_block_new(&env, 0);

		//Serialize
		xmlrpc_serialize_response(&env,output,val);

		//Free value
		xmlrpc_DECREF(val);

		//Send it
		if (!ResponseWriteBody(ses,XMLRPC_MEMBLOCK_CONTENTS(char, output), XMLRPC_MEMBLOCK_SIZE(char, output)))
			//Close on error
			break;
		//Liberamos
		XMLRPC_MEMBLOCK_FREE(char, output);

		//Remove event from queue
		queue->PopEvent();
	}

	//End it
	ResponseWriteEnd(ses);

	//Dec queue usage
	queue->DecUse();

	xmlrpc_env_clean(&env);

	Log("<ProcessRequest [time:%llu]\n",getDifTime(&tv)/1000);

	return 1;
}

