#include <stdlib.h>
#include <map>
#include <cstring>
#include "log.h"
#include "mcu.h"
#include "rtmpparticipant.h"
#include "websockets.h"


/**************************************
* MCU
*	Constructor
**************************************/
MCU::MCU()
{
	//Inicializamos los mutex
	pthread_mutex_init(&mutex,NULL);
	//No event mngr
	eventMngr = NULL;
}

/**************************************
* ~MCU
*	Destructur
**************************************/
MCU::~MCU()
{
	//End just in case
	End();
	//Destruimos el mutex
	pthread_mutex_destroy(&mutex);
}


/**************************************
* Init
*	Inicializa la mcu
**************************************/
int MCU::Init(XmlStreamingHandler *eventMngr)
{
	timeval tv;
	
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Estamos iniciados
	inited = true;

	//Get secs
	gettimeofday(&tv,NULL);
	
	//El id inicial
	maxId = (tv.tv_sec & 0x7FFF) << 16;

	//Store event mngr
	this->eventMngr = eventMngr;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return 1;
}

/**************************************
* End
*	Termina la mcu
**************************************/
int MCU::End()
{
	Log(">End MCU\n");

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Dejamos de estar iniciados
	inited = false;

	//Paramos las conferencias
	for (Conferences::iterator it=conferences.begin(); it!=conferences.end(); ++it)
	{
		//Get the entry
		ConferenceEntry *entry = it->second;

		//Get the conference
		MultiConf *multi = entry->conf;

		//End it
		multi->End();

		//Delete conference
		delete multi;

		//Delete entry
		delete entry;
	}

	//LImpiamos las listas
	conferences.clear();

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<End MCU\n");

	//Salimos
	return false;
}

int MCU::CreateEventQueue()
{
	//Check mngr
	if (!eventMngr)
		//Error
		return Error("Event manager not set!\n");

	//Create it
	int queueId = eventMngr->CreateEventQueue();
	//Add to list
	eventQueues.insert(queueId);
	//return it
	return queueId;
}

int MCU::DeleteEventQueue(int queueId)
{
	//Check mngr
	if (!eventMngr)
		//Error
		return Error("Event manager not set!\n");

	//Delete from set
	eventQueues.erase(queueId);

	//Delete it
	return eventMngr->DestroyEventQueue(queueId);
}

/**************************************
* CreateConference
*	Inicia una conferencia
**************************************/
int MCU::CreateConference(std::wstring tag,int queueId)
{
	//Log
	Log(">CreateConference [tag:%ls,queueId:%d]\n",tag.c_str(),queueId);

	//Create the multiconf
	MultiConf * conf = new MultiConf(tag);

	//Create the entry
	ConferenceEntry *entry = new ConferenceEntry();

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get the id
	int confId = maxId++;

	//Guardamos los datos
	entry->id 	= confId;
	entry->conf 	= conf;
	entry->enabled 	= 1;
	entry->numRef	= 0;
	entry->queueId	= queueId;

	//a�adimos a la lista
	conferences[confId] = entry;
	//Add to tags
	tags[tag] = confId;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Set us as listeners
	conf->SetListener(this,(void*)entry);
		
	Log("<CreateConference [%d]\n",confId);

	return confId;
}

/**************************************
* GetConferenceRef
*	Obtiene una referencia a una conferencia
**************************************/
int MCU::GetConferenceRef(int id,MultiConf **conf)
{
	Log("-GetConferenceRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find confernce
	Conferences::iterator it = conferences.find(id);

	//SI no esta
	if (it==conferences.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Conference not found [%d]\n",id);
	}

	//Get entry
	ConferenceEntry *entry = it->second;

	//Check it is enabled
	if (!entry->enabled)
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Conference not enabled [%d]\n",id);
	}

	//Aumentamos el contador
	entry->numRef++;

	//Y obtenemos el puntero a la la conferencia
	*conf = entry->conf;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	return true;
}

/**************************************
* GetConferenceID
*	Get conference Id by tag
**************************************/
int MCU::GetConferenceId(const std::wstring& tag)
{
	Log("-GetConferenceId [%ls]\n",tag.c_str());

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find id by tag
	ConferenceTags::iterator it = tags.find(tag);

	//Check if found
	if (it==tags.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Conference tag not found [%ls]\n",tag.c_str());
	}

	//Get id
	int confId = it->second;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	return confId;
}

/**************************************
* ReleaseConferenceRef
*	Libera una referencia a una conferencia
**************************************/
int MCU::ReleaseConferenceRef(int id)
{
	Log(">ReleaseConferenceRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find confernce
	Conferences::iterator it = conferences.find(id);

	//SI no esta
	if (it==conferences.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Conference not found [%d]\n",id);
	}

	//Get entry
	ConferenceEntry *entry = it->second;

	//Aumentamos el contador
	entry->numRef--;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<ReleaseConferenceRef\n");

	return true;
}

/**************************************
* DeleteConference
*	Inicializa la mcu
**************************************/
int MCU::DeleteConference(int id)
{
	Log(">DeleteConference [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find conference
	Conferences::iterator it = conferences.find(id);

	//Check if we found it or not
	if (it==conferences.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);

		//Y salimos
		return Error("Conference not found [%d]\n",id);
	}

	//Get entry
	ConferenceEntry* entry = it->second;

	Log("-Disabling conference [%d]\n",entry->enabled);

	//Disable it
	entry->enabled = 0;

	//Remove tag
	tags.erase(entry->conf->GetTag());

	//Whait to get free
	while(entry->numRef>0)
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//FIXME: poner una condicion
		sleep(20);
		//Bloqueamos
		pthread_mutex_lock(&mutex);
		//Find conference
		it = conferences.find(id);
		//Check if we found it or not
		if (it==conferences.end())
		{
			//Desbloquamos el mutex
			pthread_mutex_unlock(&mutex);
			//Y salimos
			return Error("Conference not found [%d]\n",id);
		}
		//Get entry again
		entry = it->second;
	}

	//Get conference from ref entry
	MultiConf *conf = entry->conf;

	//Remove from list
	conferences.erase(it);

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	//End conference
	conf->End();

	//Delete conference
	delete conf;

	//Delete the entry
	delete entry;

	Log("<DeleteConference [%d]\n",id);

	//Exit
	return true;
}

RTMPNetConnection* MCU::Connect(const std::wstring& appName,RTMPNetConnection::Listener* listener)
{
	int confId = 0;
	MultiConf *conf = NULL;
	wchar_t *stopwcs;
	
	//Skip the mcu part and find the conf Id
	int i = appName.find(L"/");

	//Check if
	if (i<0)
	{
		//Noting found
		Error("Wrong format for app name\n");
		//Exit
		return NULL;
	}

	//Get type
	std::wstring type = appName.substr(0,i);

	//Get arg
	std::wstring arg = appName.substr(i+1);

	//Check type
	if (type.compare(L"mcutag")==0)
		//Get by tag
		confId = GetConferenceId(arg);
	else
		//Fet conf Id
		confId = wcstol(arg.c_str(),&stopwcs,10);

	//Get conference
	if(!GetConferenceRef(confId,&conf))
	{
		//No conference found
		Error("Conference not found [confId:%d]\n",confId);
		//Exit
		return NULL;
	}

	//Connect
	conf->Connect(listener);
	
	//release it
	ReleaseConferenceRef(confId);

	//Return conf
	return conf;
}

int MCU::GetConferenceList(ConferencesInfo& lst)
{
	Log(">GetConferenceList\n");

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//For each conference
	for (Conferences::iterator it = conferences.begin(); it!=conferences.end(); ++it)
	{
		//Get entry
		ConferenceEntry *entry = it->second;

		//Check it is enabled
		if (entry->enabled)
		{
			ConferenceInfo info;
			//Set data
			info.id = entry->id;
			info.name = entry->conf->GetTag();
			info.numPart = entry->conf->GetNumParticipants();
			//Append new info
			lst[entry->id] = info;
		}
	}
	
	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<GetConferenceList\n");

	return true;
}

void MCU::onParticipantRequestFPU(MultiConf *conf,int partId,void *param)
{
	ConferenceEntry *entry = (ConferenceEntry *)param;
	//Check Event and event queue
	if (eventMngr && entry->queueId>0)
		//Send new event
		eventMngr->AddEvent(entry->queueId, new ::PlayerRequestFPUEvent(entry->id,conf->GetTag(),partId));
}

void MCU::onCPULoad(int user, int sys, int load, int numcpu)
{
	//Check Event and event queue
	if (eventMngr)
		//For each queue
		for (EventQueues::iterator it=eventQueues.begin();it!=eventQueues.end();++it)
			//Send new event
			eventMngr->AddEvent(*it, new CPULoadInfoEvent(user,sys,load,numcpu));
}

int MCU::onFileUploaded(const char* url, const char *filename)
{
	Log("-File upload for %s\n",url);
	
	MultiConf* conf = NULL;

	//Skip the first path
	const char *sep = url + strlen("/upload/mcu/app/");

	//If not found
	if (!sep)
		//not found
		return 404;

	//Convert to wstring
	UTF8Parser parser;

	if (!parser.Parse((BYTE*)sep,strlen(sep)))
	{
		//Error
		Error("Error parsing conference tag\n");
		//Error
		return 500;
	}
	
	//Get id by tag
	int confId = GetConferenceId(parser.GetWString());

	//Get conference
	if(!GetConferenceRef(confId,&conf))
	{
		//Error
		Error("Conference does not exist\n");
		//Not found
		return 404;
	}

	//Display it
	int ret = conf->AppMixerDisplayImage(filename) ? 200 : 500;

	//Release it
	ReleaseConferenceRef(confId);

	//REturn result
	return ret;
}

void MCU::onWebSocketConnection(const HTTPRequest& request,WebSocket *ws)
{
	MultiConf* conf = NULL;

	//Log
	Log("-WebSocket connection for for %s\n",request.GetRequestURI().c_str());

	//Get url
	std::string url = request.GetRequestURI();

	StringParser parser(url);

	//Check it is for us
	if(!parser.MatchString("/mcu/"))
	{
		//reject
		ws->Reject(400,"Bad request");
		//Not found
		return;
	}

	//Get until next /
	if (!parser.ParseUntilCharset("/"))
	{
		//reject
		ws->Reject(400,"Bad request");
		//Not found
		return;
	}
	
	//Get value
	std::string value = parser.GetValue();

	//Get id by tag
	int confId = GetConferenceId(std::wstring(value.begin(),value.end()));

	//If not found
	if (!confId)
	{
		//reject
		ws->Reject(404,"Conference does not exist");
		//Error
		Error("Conference does not exist\n");
		//Not found
		return;
	}

	//Skip /
	parser.Next();

	//Get participant id
	if (!parser.ParseUntilCharset("/?"))
	{
		//reject
		ws->Reject(400,"Bad request");
		//Not found
		return;
	}

	//Get value
	int partId = atoi(parser.GetValue().c_str());

	//Viewer only by default
	bool isPresenter = false;

	//Check if it is presented
	if (parser.CheckString("/presenter"))
		//It is presenter
		isPresenter = true;

	//Get conference
	if(!GetConferenceRef(confId,&conf))
	{
		//reject
		ws->Reject(404,"Conference does not exist");
		//Error
		Error("Conference does not exist\n");
		//Not found
		return;
	}

	//Connect it
	conf->AppMixerWebsocketConnectRequest(partId,ws,isPresenter);

	//Release it
	ReleaseConferenceRef(confId);
}
