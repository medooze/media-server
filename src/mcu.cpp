#include <stdlib.h>
#include <map>
#include <cstring>
#include "log.h"
#include "mcu.h"
#include "rtmp/rtmpparticipant.h"
#include "ws/websockets.h"


/**************************************
* MCU
*	Constructor
**************************************/
MCU::MCU()
{
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
}


/**************************************
* Init
*	Inicializa la mcu
**************************************/
int MCU::Init(XmlStreamingHandler *eventMngr)
{
	timeval tv;

	//Lock
	use.WaitUnusedAndLock();

	//Estamos iniciados
	inited = true;

	//Get secs
	gettimeofday(&tv,NULL);

	//El id inicial
	maxId = (tv.tv_sec & 0x7FFF) << 16;

	//Store event mngr
	this->eventMngr = eventMngr;

	//Unlock
	use.Unlock();

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

	//Lock
	use.WaitUnusedAndLock();

	//Dejamos de estar iniciados
	inited = false;

	//Paramos las conferencias
	for (Conferences::iterator it=conferences.begin(); it!=conferences.end(); ++it)
	{
		//Get the entry
		ConferenceEntry *entry = it->second;

		//Get the conference
		MultiConf::shared multi = entry->conf;

		//End it
		multi->End();

		//Delete entry
		delete entry;
	}

	//LImpiamos las listas
	conferences.clear();

	//Unlock
	use.Unlock();

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
	std::shared_ptr<MultiConf> conf = std::make_shared<MultiConf>(tag);

	//Create the entry
	ConferenceEntry *entry = new ConferenceEntry();

	//Lock
	use.WaitUnusedAndLock();

	//Get the id
	int confId = maxId++;

	//Guardamos los datos
	entry->id 	= confId;
	entry->conf 	= conf;
	entry->enabled 	= 1;
	entry->queueId	= queueId;

	//Add to the conference entry list
	conferences[confId] = entry;
	//Add to tags
	tags[tag] = confId;

	//Unlock
	use.Unlock();

	//Set us as listeners
	conf->SetListener(this,(void*)entry);

	Log("<CreateConference [%d]\n",confId);

	return confId;
}

/**************************************
* GetConferenceRef
*	Obtiene una referencia a una conferencia
**************************************/
MultiConf::shared MCU::GetConferenceRef(int id)
{
	Log("-GetConferenceRef [%d]\n",id);

	//Inc usage
	use.IncUse();

	//Find confernce
	Conferences::iterator it = conferences.find(id);

	//SI no esta
	if (it==conferences.end())
	{
		//Desbloquamos el mutex
		use.DecUse();
		//Error
		Error("Conference not found [%d]\n",id);
		//No conference
		return nullptr;
	}

	//Get entry
	ConferenceEntry *entry = it->second;

	//Check it is enabled
	if (!entry->enabled)
	{
		//Desbloquamos el mutex
		use.DecUse();
		//Error
		Error("Conference not enabled [%d]\n", id);
		//No conference
		return nullptr;
	}

	//Inc usage
	entry->IncUse();

	//Get pointer to conference
	MultiConf::shared conf = entry->conf;

	//Desbloquamos el mutex
	use.DecUse();

	return conf;
}

/**************************************
* GetConferenceID
*	Get conference Id by tag
**************************************/
int MCU::GetConferenceId(const std::wstring& tag)
{
	Log("-GetConferenceId [%ls]\n",tag.c_str());

	//Lock
	use.IncUse();

	//Find id by tag
	ConferenceTags::iterator it = tags.find(tag);

	//Check if found
	if (it==tags.end())
	{
		//Desbloquamos el mutex
		use.DecUse();
		//Y salimos
		return Error("Conference tag not found [%ls]\n",tag.c_str());
	}

	//Get id
	int confId = it->second;

	//Desbloquamos el mutex
	use.DecUse();

	return confId;
}

/**************************************
* ReleaseConferenceRef
*	Libera una referencia a una conferencia
**************************************/
int MCU::ReleaseConferenceRef(int id)
{
	Log(">ReleaseConferenceRef [%d]\n",id);

	//Lock
	use.IncUse();

	//Find confernce
	Conferences::iterator it = conferences.find(id);

	//SI no esta
	if (it==conferences.end())
	{
		//Desbloquamos el mutex
		use.DecUse();
		//Y salimos
		return Error("Conference not found [%d]\n",id);
	}

	//Get entry
	ConferenceEntry *entry = it->second;

	//Free ref
	entry->DecUse();

	//Desbloquamos el mutex
	use.DecUse();

	Log("<ReleaseConferenceRef\n");

	return true;
}

/**************************************
* DeleteConference
*	Removes the mcu
**************************************/
int MCU::DeleteConference(int id)
{
	Log(">DeleteConference [%d]\n",id);

	//Lock
	use.WaitUnusedAndLock();

	//Find conference
	Conferences::iterator it = conferences.find(id);

	//Check if we found it or not
	if (it==conferences.end())
	{
		//Desbloquamos el mutex
		use.Unlock();
		//Y salimos
		return Error("Conference not found [%d]\n",id);
	}

	//Get entry
	ConferenceEntry* entry = it->second;
	
	//Check if it was already disabled
	if (!entry->enabled)
	{
		//Desbloquamos el mutex
		use.Unlock();
		//Y salimos
		return Error("Conference already disabled [%d]\n",id);
	}

	//Log
	Log("-Disabling conference [%d]\n",entry->enabled);

	//Disable it
	entry->enabled = 0;

	//Remove tag
	tags.erase(entry->conf->GetTag());
	
	//Free list
	use.Unlock();

	//Wait conf to get released
	entry->WaitUnusedAndLock();
	
	//Lock again
	use.WaitUnusedAndLock();
	
	//Get conference from ref entry
	MultiConf::shared conf = entry->conf;

	//Remove from list
	conferences.erase(it);
	
	//Desbloquamos el mutex
	use.Unlock();

	//End conference
	conf->End();

	//Delete the entry
	delete entry;

	Log("<DeleteConference [%d]\n",id);

	//Exit
	return true;
}

RTMPNetConnection::shared MCU::Connect(const std::wstring& appName,RTMPNetConnection::Listener *listener,std::function<void(bool)> accept)
{
	int confId = 0;
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
	auto conf = GetConferenceRef(confId);
	//If not found
	if(!conf)
	{
		//No conference found
		Error("Conference not found [confId:%d]\n",confId);
		//Exit
		return NULL;
	}

	//Connect
	conf->AddListener(listener);
	
	//Accept it
	accept(true);

	//release it
	ReleaseConferenceRef(confId);

	//Return conf
	return conf;
}

int MCU::GetConferenceList(ConferencesInfo& lst)
{
	Log(">GetConferenceList\n");

	//Lock
	use.IncUse();

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
			Debug("-Entry [id:%d,name%ls,parts:%d\n",info.id,entry->conf->GetTag().c_str(),info.numPart);
		}
	}

	//Desbloquamos el mutex
	use.DecUse();

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
	auto conf = GetConferenceRef(confId);
	//If not found
	if (!conf)
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
	//Log
	Log("-WebSocket connection for for %s\n",request.GetRequestURI().c_str());

	//Get url
	std::string url = request.GetRequestURI();

	StringParser parser(url);

	//Check it is for us
	if (!parser.MatchString("/mcu/"))
		//reject
		return ws->Reject(400,"Bad request");

	//Get until next /
	if (!parser.ParseUntilCharset("/"))
		//reject
		return ws->Reject(400,"Bad request");

	//Get conf id value
	std::string value = parser.GetValue();

	//Get id by tag
	int confId = GetConferenceId(std::wstring(value.begin(),value.end()));

	//If not found
	if (!confId)
	{
		//Error
		Error("Conference does not exist\n");
		//reject
		return ws->Reject(404,"Conference does not exist");
	}

	//Skip /
	parser.Next();

	//Get participant id
	if (!parser.ParseUntilCharset(":"))
		//reject
		return ws->Reject(400,"Bad request");

	//Get value
	int partId = atoi(parser.GetValue().c_str());

	//Skip :
	parser.Next();

	//Get participant id
	if (!parser.ParseUntilCharset("/"))
		//reject
		return ws->Reject(400,"Bad request");

	//Get to what are they connecting
	std::string token = parser.GetValue();

	//Skip /
	parser.Next();

	//Get participant id
	if (!parser.ParseUntilCharset("/"))
		//reject
		return ws->Reject(400,"Bad request");

	//Get to what are they connecting
	std::string to = parser.GetValue();

	//Get conference
	auto conf = GetConferenceRef(confId);

	//If not found
	if(!conf)
	{
		//Error
		Error("Conference does not exist\n");
		//reject
		return ws->Reject(404,"Conference does not exist");
	}

	//Connect it
	conf->WebsocketConnectRequest(ws,partId,token,to);

	//Release it
	ReleaseConferenceRef(confId);
}
