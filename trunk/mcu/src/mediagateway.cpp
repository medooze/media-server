/* 
 * File:   mediagateway.cpp
 * Author: Sergio
 * 
 * Created on 22 de diciembre de 2010, 18:10
 */
#include "log.h"
#include "mediagateway.h"

MediaGateway::MediaGateway()
{
	//Inicializamos los mutex
	pthread_mutex_init(&mutex,NULL);
}

MediaGateway::~MediaGateway()
{
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}


/**************************************
* Init
*	Inititalize the media gateway server
**************************************/
bool MediaGateway::Init()
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Estamos iniciados
	inited = true;

	//El id inicial
	maxId=100;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return inited;
}

/**************************************
* End
*	Ends media gateway server
**************************************/
bool MediaGateway::End()
{
	Log(">End MediaGateway\n");

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Dejamos de estar iniciados
	inited = false;

	//Paramos las sesions
	for (MediaBridgeEntries::iterator it=bridges.begin(); it!=bridges.end(); it++)
	{
		//Obtenemos la sesion
		MediaBridgeSession *session = it->second.session;

		//La paramos
		session->End();

		//Y la borramos
		delete session;
	}

	//Clear the MediaGateway list
	bridges.clear();

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<End MediaGateway\n");

	//Salimos
	return !inited;
}

/**************************************
* CreateMediaBridge
*	Create a media bridge session
**************************************/
DWORD MediaGateway::CreateMediaBridge(const std::wstring &name)
{
	Log("-CreateBroadcast [name:\"%ls\"]\n",name.c_str());


	//Creamos la session
	MediaBridgeSession * session = new MediaBridgeSession();

	//Obtenemos el id
	DWORD sessionId = maxId++;

	//Creamos la entrada
	MediaBridgeEntry entry;

	//Guardamos los datos
	entry.id 	= sessionId;
	entry.name 	= name;
        entry.session 	= session;
	entry.enabled 	= 1;
	entry.numRef	= 0;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//a�adimos a la lista
	bridges[sessionId] = entry;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return sessionId;
}

/**************************************
 * SetMediaBridgeInputToken
 *	Associates a token with a media bridge for input
 *	In case there is already a pin associated with that session it fails.
 **************************************/
bool MediaGateway::SetMediaBridgeInputToken(DWORD id,const std::wstring &token)
{
	Log(">SetMediaBridgeInputToken [id:%d,token:\"%ls\"]\n",id,token.c_str());

	bool res = false;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get the MediaGateway session entry
	MediaBridgeEntries::iterator it = bridges.find(id);

	//Check it
	if (it==bridges.end())
	{
		//Broadcast not found
		Error("Media bridge session not found\n");
		//Exit
		goto end;
	}

	//Add it
	it->second.session->AddInputToken(token);

	//Everything was ok
	res = true;

end:
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<SetMediaBridgeInputToken\n");

	return res;
}

/**************************************
 * SetMediaBridgeOutputToken
 *	Associates a token with a media bridge for input
 *	In case there is already a pin associated with that session it fails.
 **************************************/
bool MediaGateway::SetMediaBridgeOutputToken(DWORD id,const std::wstring &token)
{
	Log(">SetMediaBridgeOutputToken [id:%d,token:\"%ls\"]\n",id,token.c_str());

	bool res = false;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get the MediaGateway session entry
	MediaBridgeEntries::iterator it = bridges.find(id);

	//Check it
	if (it==bridges.end())
	{
		//Broadcast not found
		Error("Media bridge session not found\n");
		//Exit
		goto end;
	}

	//Add it
	it->second.session->AddOutputToken(token);

	//Everything was ok
	res = true;

end:
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<SetMediaBridgeOutputToken\n");

	return res;
}

/**************************************
* GetMediaBridgeRef
*	Obtiene una referencia a una sesion
**************************************/
bool MediaGateway::GetMediaBridgeRef(DWORD id,MediaBridgeSession **session)
{
	Log(">GetMediaBridgeRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get it
	MediaBridgeEntries::iterator it = bridges.find(id);

	//SI no esta
	if (it==bridges.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Session no encontrada [%d]\n",id);
	}

	//Get bridge entry
	MediaBridgeEntry &entry = it->second;

	//Increase counter reference
	entry.numRef++;

	//Y obtenemos el puntero a la la sesion
	*session = entry.session;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<GetMediaBridgeRef \n");

	return true;
}

/**************************************
* ReleaseMediaBridgeRef
*	Libera una referencia a una sesion
**************************************/
bool MediaGateway::ReleaseMediaBridgeRef(DWORD id)
{
	Log(">ReleaseMediaBridgeRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get it
	MediaBridgeEntries::iterator it = bridges.find(id);

	//SI no esta
	if (it==bridges.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Session no encontrada [%d]\n",id);
	}

	//Get bridge entry
	MediaBridgeEntry &entry = it->second;

	//Decrease counter
	entry.numRef--;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<ReleaseMediaBridgeRef\n");

	return true;
}

/**************************************
* DeleteSession
*	Inicializa el servidor de FLV
**************************************/
bool MediaGateway::DeleteMediaBridge(DWORD id)
{
	Log(">DeleteMediaBridge [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find sessionerence
	MediaBridgeEntries::iterator it = bridges.find(id);

	//Check if we found it or not
	if (it==bridges.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);

		//Y salimos
		return Error("Session no encontrada [%d]\n",id);
	}

	//TODO: Wait for numref == 0

	//Get entry
	MediaBridgeEntry &entry = it->second;

	//Get sessionerence
	MediaBridgeSession *session = entry.session;

	//Remove entry from list
	bridges.erase(it);

	Log("-Ending session [%d]\n",id);

	//End session
	session->End();

	//Delete sessionerence
	delete session;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<DeleteMediaBridge [%d]\n",id);

	//Exit
	return true;
}

RTMPNetConnection* MediaGateway::Connect(const std::wstring& appName,RTMPNetConnection::Listener* listener)
{
	MediaBridgeSession *sess = NULL;
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

	//Get conf id
	int confId = wcstol(appName.substr(i+1).c_str(),&stopwcs,10);

	//Get conference
	if(!GetMediaBridgeRef(confId,&sess))
	{
		//No conference found
		Error("MediaBridge not found [confId:%d]\n",confId);
		//Exit
		return NULL;
	}

	//Connect
	sess->Connect(listener);

	//release it
	ReleaseMediaBridgeRef(confId);

	//Return conf
	return sess;
}
