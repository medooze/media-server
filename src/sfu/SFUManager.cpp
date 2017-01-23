#include "log.h"
#include "use.h"
#include "amf.h"
#include "SFUManager.h"
#include "xmlstreaminghandler.h"

using namespace SFU;

SFUManager::SFUManager()
{
	//Init id
	maxId = 1;
	//NO manager
	eventMngr = NULL;
	//Create mutex
	pthread_mutex_init(&mutex,NULL);
}

SFUManager::~SFUManager()
{
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}


/**************************************
* Init
*	Inicializa la SFUManager
**************************************/
int SFUManager::Init(XmlStreamingHandler *eventMngr)
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
*	Termina la SFUManager
**************************************/
int SFUManager::End()
{
	Log(">End SFUManager\n");

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Dejamos de estar iniciados
	inited = false;

	//Paramos las conferencias
	for (Rooms::iterator it=rooms.begin(); it!=rooms.end(); ++it)
	{
		//Obtenemos la conferencia
		Room *room = it->second->room;
		
		//End media rooms
		room->End();

		//Delete object
		delete room;

		//Delete also the entry
		delete (it->second);
	}

	//LImpiamos las listas
	rooms.clear();

	//NO manager
	eventMngr = NULL;
	
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<End SFUManager\n");

	//Salimos
	return false;
}

int SFUManager::CreateEventQueue()
{
	//Check mngr
	if (!eventMngr)
		//Error
		return Error("Event manager not set!\n");

	//Create it
	return eventMngr->CreateEventQueue();
}

int SFUManager::DeleteEventQueue(int id)
{
	//Check mngr
	if (!eventMngr)
		//Error
		return Error("Event manager not set!\n");

	//Create it
	return eventMngr->DestroyEventQueue(id);
}


/**************************************
* CreateRoom
*	Inicia una conferencia
**************************************/
int SFUManager::CreateRoom(std::wstring tag,int queueId)
{
	Log(">CreateRoom\n");

	//Obtenemos el id
	int roomId = maxId++;

	//Creamos la multi
	Room * room = new Room(tag);

	//Creamos la entrada
	RoomEntry* entry = new RoomEntry();

	//Guardamos los datos
	entry->id 	= roomId;
	entry->tag 	= tag;
	entry->room 	= room;
	entry->queueId	= queueId;
	entry->enabled 	= 1;

	//INit it
	room->Init();
	//Bloqueamos
	
	pthread_mutex_lock(&mutex);

	//a�adimos a la lista
	rooms[roomId] = entry;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<CreateRoom [%d]\n",roomId);

	return roomId;
}

/**************************************
* GetRoomRef
*	Obtiene una referencia a una conferencia
**************************************/
int SFUManager::GetRoomRef(int id,Room **room)
{
	//Log(">GetRoomRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find confernce
	Rooms::iterator it = rooms.find(id);

	//SI no esta
	if (it==rooms.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Media room not found [%d]\n",id);
	}

	//Get entry
	RoomEntry *entry = it->second;

	//Check it is enabled
	if (!entry->enabled)
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Room not enabled [%d]\n",id);
	}

	//Aumentamos el contador
	entry->IncUse();

	//Y obtenemos el puntero a la la conferencia
	*room = entry->room;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	//Log("<GetRoomRef [%d,%d]\n",entry->enabled,it->second->enabled);

	return true;
}

/**************************************
* ReleaseRoomRef
*	Libera una referencia a una conferencia
**************************************/
int SFUManager::ReleaseRoomRef(int id)
{
//	Log(">ReleaseRoomRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find confernce
	Rooms::iterator it = rooms.find(id);

	//SI no esta
	if (it==rooms.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Media room not found [%d]\n",id);
	}

	//Get entry
	RoomEntry *entry = it->second;

	//Aumentamos el contador
	entry->DecUse();

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

//	Log("<ReleaseRoomRef\n");

	return true;
}

/**************************************
* DeleteRoom
*	Inicializa la SFUManager
**************************************/
int SFUManager::DeleteRoom(int id)
{
	Log(">DeleteRoom [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find conference
	Rooms::iterator it = rooms.find(id);

	//Check if we found it or not
	if (it==rooms.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);

		//Y salimos
		return Error("Media room not found [%d]\n",id);
	}

	//Get entry
	RoomEntry *entry = it->second;

	Log("-Disabling conference [%d]\n",entry->enabled);

	//Disable it
	entry->enabled = 0;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	//Whait to get free
	entry->WaitUnusedAndLock();

	//Get conference from ref entry
	Room *room = entry->room;

	//If still has room
	if (room)
	{
		//End conference
		room->End();

		//Delete conference
		delete room;

		//Set to null
		entry->room = NULL;
	}

	//Desbloquamos el mutex
	entry->Unlock();

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find conference again
	it = rooms.find(id);

	//Check if we found it or not
	if (it==rooms.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);

		//Y salimos
		return Error("Media room not found [%d]\n",id);
	}
	
	//Delete entry
	delete (it->second);

	//Remove from list
	rooms.erase(it);

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<DeleteRoom [%d]\n",id);

	//Exit
	return true;
}


