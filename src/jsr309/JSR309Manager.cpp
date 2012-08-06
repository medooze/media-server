/* 
 * File:   JSR309Manager.cpp
 * Author: Sergio
 * 
 * Created on 8 de septiembre de 2011, 13:06
 */
#include "log.h"
#include "use.h"
#include "amf.h"
#include "JSR309Manager.h"
#include "xmlstreaminghandler.h"


JSR309Manager::JSR309Manager()
{
	//Init id
	maxId = 1;
	//NO manager
	eventMngr = NULL;
	//Create mutex
	pthread_mutex_init(&mutex,NULL);
}

JSR309Manager::~JSR309Manager()
{
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}


/**************************************
* Init
*	Inicializa la JSR309Manager
**************************************/
int JSR309Manager::Init(XmlStreamingHandler *eventMngr)
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
*	Termina la JSR309Manager
**************************************/
int JSR309Manager::End()
{
	Log(">End JSR309Manager\n");

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Dejamos de estar iniciados
	inited = false;

	//Paramos las conferencias
	for (MediaSessions::iterator it=sessions.begin(); it!=sessions.end(); ++it)
	{
		//Obtenemos la conferencia
		MediaSession *sess = it->second->sess;

		//End media sessions
		sess->End();

		//Delete object
		delete sess;

		//Delete also the entry
		delete (it->second);
	}

	//LImpiamos las listas
	sessions.clear();

	//NO manager
	eventMngr = NULL;
	
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<End JSR309Manager\n");

	//Salimos
	return false;
}

int JSR309Manager::CreateEventQueue()
{
	//Check mngr
	if (!eventMngr)
		//Error
		return Error("Event manager not set!\n");

	//Create it
	return eventMngr->CreateEventQueue();
}

int JSR309Manager::DeleteEventQueue(int id)
{
	//Check mngr
	if (!eventMngr)
		//Error
		return Error("Event manager not set!\n");

	//Create it
	return eventMngr->DestroyEventQueue(id);
}


/**************************************
* CreateMediaSession
*	Inicia una conferencia
**************************************/
int JSR309Manager::CreateMediaSession(std::wstring tag,int queueId)
{
	Log(">CreateMediaSession\n");

	//Obtenemos el id
	int sessId = maxId++;

	//Creamos la multi
	MediaSession * sess = new MediaSession(tag);

	//Creamos la entrada
	MediaSessionEntry* entry = new MediaSessionEntry();

	//Guardamos los datos
	entry->id 	= sessId;
	entry->tag 	= tag;
	entry->sess 	= sess;
	entry->queueId	= queueId;
	entry->enabled 	= 1;

	//Set listener
	sess->SetListener(this,(void*)entry);

	//INit it
	sess->Init();
	//Bloqueamos
	
	pthread_mutex_lock(&mutex);

	//a�adimos a la lista
	sessions[sessId] = entry;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<CreateMediaSession [%d]\n",sessId);

	return sessId;
}

/**************************************
* GetMediaSessionRef
*	Obtiene una referencia a una conferencia
**************************************/
int JSR309Manager::GetMediaSessionRef(int id,MediaSession **sess)
{
	//Log(">GetMediaSessionRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find confernce
	MediaSessions::iterator it = sessions.find(id);

	//SI no esta
	if (it==sessions.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Media session not found [%d]\n",id);
	}

	//Get entry
	MediaSessionEntry *entry = it->second;

	//Check it is enabled
	if (!entry->enabled)
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("MediaSession not enabled [%d]\n",id);
	}

	//Aumentamos el contador
	entry->IncUse();

	//Y obtenemos el puntero a la la conferencia
	*sess = entry->sess;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	//Log("<GetMediaSessionRef [%d,%d]\n",entry->enabled,it->second->enabled);

	return true;
}

/**************************************
* ReleaseMediaSessionRef
*	Libera una referencia a una conferencia
**************************************/
int JSR309Manager::ReleaseMediaSessionRef(int id)
{
//	Log(">ReleaseMediaSessionRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find confernce
	MediaSessions::iterator it = sessions.find(id);

	//SI no esta
	if (it==sessions.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Media session not found [%d]\n",id);
	}

	//Get entry
	MediaSessionEntry *entry = it->second;

	//Aumentamos el contador
	entry->DecUse();

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

//	Log("<ReleaseMediaSessionRef\n");

	return true;
}

/**************************************
* DeleteMediaSession
*	Inicializa la JSR309Manager
**************************************/
int JSR309Manager::DeleteMediaSession(int id)
{
	Log(">DeleteMediaSession [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find conference
	MediaSessions::iterator it = sessions.find(id);

	//Check if we found it or not
	if (it==sessions.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);

		//Y salimos
		return Error("Media session not found [%d]\n",id);
	}

	//Get entry
	MediaSessionEntry *entry = it->second;

	Log("-Disabling conference [%d]\n",entry->enabled);

	//Disable it
	entry->enabled = 0;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	//Whait to get free
	entry->WaitUnusedAndLock();

	//Get conference from ref entry
	MediaSession *sess = entry->sess;

	//If still has session
	if (sess)
	{
		//End conference
		sess->End();

		//Delete conference
		delete sess;

		//Set to null
		entry->sess = NULL;
	}

	//Desbloquamos el mutex
	entry->Unlock();

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find conference again
	it = sessions.find(id);

	//Check if we found it or not
	if (it==sessions.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);

		//Y salimos
		return Error("Media session not found [%d]\n",id);
	}
	
	//Delete entry
	delete (it->second);

	//Remove from list
	sessions.erase(it);

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<DeleteMediaSession [%d]\n",id);

	//Exit
	return true;
}

void JSR309Manager::onPlayerEndOfFile(MediaSession *sess,Player *player,int playerId,void *param)
{
	//Get media session entry
	MediaSessionEntry *entry = (MediaSessionEntry *)param;

	//Check streamers
	if (eventMngr)
		//Send new event
		eventMngr->AddEvent(entry->queueId, new ::PlayerEndOfFileEvent(sess->GetTag(),player->GetTag()));
}

