#include "log.h"
#include "broadcaster.h"


/**************************************
* Broadcaster
*	Constructor
**************************************/
Broadcaster::Broadcaster() 
{
	//Inicializamos los mutex
	pthread_mutex_init(&mutex,NULL);
}

/**************************************
* ~Broadcaster
*	Destructur
**************************************/
Broadcaster::~Broadcaster()
{
	//Destruimos el mutex
	pthread_mutex_destroy(&mutex);
}


/**************************************
* Init
*	Inicializa el servidor de FLV
**************************************/
bool Broadcaster::Init()
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
*	Termina el servidor de FLV
**************************************/
bool Broadcaster::End()
{
	Log(">End Broadcaster\n");

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Dejamos de estar iniciados
	inited = false;

	//Paramos las sesions
	for (BroadcastEntries::iterator it=broadcasts.begin(); it!=broadcasts.end(); it++)
	{
		//Obtenemos la sesion
		BroadcastSession *session = it->second.session;

		//La paramos
		session->End();

		//Y la borramos
		delete session;
	}

	//Clear the broadcaster list
	broadcasts.clear();

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<End Broadcaster\n");

	//Salimos
	return !inited;
}

/**************************************
* CreateBroadcast
*	Inicia una sesion
**************************************/
DWORD Broadcaster::CreateBroadcast(const std::wstring &name,const std::wstring &tag)
{
	Log("-CreateBroadcast [name:\"%ls\",tag:\"%ls\"]\n",name.c_str(),tag.c_str());


	//Creamos la session
	BroadcastSession * session = new BroadcastSession(tag);

	//Obtenemos el id
	DWORD sessionId = maxId++;

	//Creamos la entrada
	BroadcastEntry entry; 

	//Guardamos los datos
	entry.id 	= sessionId;
	entry.name 	= name;
        entry.tag       = tag;
	entry.session 	= session;
	entry.enabled 	= 1;
	entry.numRef	= 0;
	
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//a�adimos a la lista
	broadcasts[sessionId] = entry;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return sessionId;
}

/**************************************
 * PublishBroadcast
 *  
 **************************************/
bool Broadcaster::PublishBroadcast(DWORD id,const std::wstring &pin)
{
	Log(">PublishBroadcast [id:%d,pin:\"%ls\"]\n",id,pin.c_str());

	bool res = false;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get the broadcaster session entry
	BroadcastEntries::iterator it = broadcasts.find(id);

	//Check it
	if (it==broadcasts.end())
	{
		//Broadcast not found
		Error("Broadcast session not found\n");
		//Exit
		goto end;
	}

	//Check if it was already been published
	if (!it->second.pin.empty())
	{
		//Broadcast not found
		Error("Broadcast session already published\n");
		//Exit
		goto end;
	}

	//Check if the pin is already in use
	if (published.find(pin)!=published.end())
	{
		//Broadcast not found
		Error("Pin already in use by other broadcast session\n");
		//Exit
		goto end;
	}

	//Add to the pin list
	published[pin] = id;

	//Set the entry broadcast session publication pin
	it->second.pin = pin;

	//Everything was ok
	res = true;

end:
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<PublishBroadcast\n");

	return res;
}

/********************************************************
 * AddBroadcastToken
 *   Add a token for playing
 ********************************************************/
bool Broadcaster::AddBroadcastToken(DWORD id, const std::wstring &token)
{
	Log("-AddBroadcastToken [id:%d,token:\"%ls\"]\n",id,token.c_str());

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Check if the pin is already in use
	if (tokens.find(token)!=tokens.end())
	{
		//Desbloqueamos
		pthread_mutex_unlock(&mutex);
		//Broadcast not found
		return Error("Token already in use\n");
	}

	//Add to the pin list
	tokens[token] = id;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return true;
}

/**************************************
 * UnPublishBroadcast
 *  
 **************************************/
bool Broadcaster::UnPublishBroadcast(DWORD id)
{
	Log(">UnPublishBroadcast [id:%d]\n",id);

	bool res = false;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get the broadcaster session entry
	BroadcastEntries::iterator it = broadcasts.find(id);

	//Check it
	if (it==broadcasts.end())
	{
		//Broadcast not found
		Error("Broadcast session not found\n");
		//Exit
		goto end;
	}

	//Check if it was already been published
	if (it->second.pin.empty())
	{
		//Broadcast not found
		Error("Broadcast session was not published\n");
		//Exit
		goto end;
	}

	//Remove pin
	published.erase(it->second.pin);

	//Set the entry broadcast session publication pin empty
	it->second.pin = L"";

	//Everything was ok
	res = true;

end:
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<PublishBroadcast\n");

	return res;
}

/**************************************
 * GetPublishedBroadcastId
 *   Get the id of the session published with the pin
 * ************************************/
DWORD Broadcaster::GetPublishedBroadcastId(const std::wstring &pin)
{
	DWORD sessionId = 0;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Check if there is already a pin like that
	PublishedBroadcasts::iterator it = published.find(pin);

	//Check we found one
	if (it!=published.end())
		//Get id
		sessionId = it->second;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Return session id
	return sessionId;
}
DWORD Broadcaster::GetTokenBroadcastId(const std::wstring &token)
{
	Log(">GetTokenBroadcastId [token:\"%ls\"]\n",token.c_str());

	DWORD sessionId = 0;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Check if there is already a pin like that
	BroadcastTokens::iterator it = tokens.find(token);

	//Check we found one
	if (it!=tokens.end())
	{
		//Get id
		sessionId = it->second;
		//Remove token
		tokens.erase(it);
	}

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	Log("<GetTokenBroadcastId [sessionId:\"%d\"]\n",sessionId);

	//Return session id
	return sessionId;
}

/**************************************
* GetBroadcastRef
*	Obtiene una referencia a una sesion
**************************************/
bool Broadcaster::GetBroadcastRef(DWORD id,BroadcastSession **session)
{
	Log(">GetBroadcastRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find session reference
	BroadcastEntries::iterator it = broadcasts.find(id);

	//SI no esta
	if (it==broadcasts.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Session no encontrada [%d]\n",id);
	}

	//Get entry
	BroadcastEntry &entry = it->second;

	//Aumentamos el contador
	entry.numRef++;

	//Y obtenemos el puntero a la la sesion
	*session = entry.session;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<GetBroadcastRef \n");

	return true;
}

/**************************************
* ReleaseBroadcastRef
*	Libera una referencia a una sesion
**************************************/
bool Broadcaster::ReleaseBroadcastRef(DWORD id)
{
	Log(">ReleaseBroadcastRef [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find session reference
	BroadcastEntries::iterator it = broadcasts.find(id);

	//SI no esta
	if (it==broadcasts.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//Y salimos
		return Error("Session no encontrada [%d]\n",id);
	}

	//Get entry
	BroadcastEntry &entry = it->second;

	//Aumentamos el contador
	entry.numRef--;

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	Log("<ReleaseBroadcastRef\n");

	return true;
}

/**************************************
* DeleteSession
*	Inicializa el servidor de FLV
**************************************/
bool Broadcaster::DeleteBroadcast(DWORD id)
{
	Log(">DeleteSession [%d]\n",id);

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Find sessionerence
	BroadcastEntries::iterator it = broadcasts.find(id);

	//Check if we found it or not
	if (it==broadcasts.end())
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);

		//Y salimos
		return Error("Session no encontrada [%d]\n",id);
	}

	//Get entry
	BroadcastEntry &entry = it->second;

	//Whait to get free
	while(entry.numRef>0)
	{
		//Desbloquamos el mutex
		pthread_mutex_unlock(&mutex);
		//FIXME: poner una condicion
		sleep(20);
		//Bloqueamos
		pthread_mutex_lock(&mutex);
		//Find broadcast
		it =  broadcasts.find(id);
		//Check if we found it or not
		if (it==broadcasts.end())
		{
			//Desbloquamos el mutex
			pthread_mutex_unlock(&mutex);
			//Y salimos
			return Error("Broadcast no encontrada [%d]\n",id);
		}
		//Get entry again
		entry = it->second;
	}

	//Get sessionerence
	BroadcastSession *session = entry.session;

	//If it was published
	if (!entry.pin.empty())
		//Remove from published list
		published.erase(entry.pin);

	//Remove from list
	broadcasts.erase(it);

	//Desbloquamos el mutex
	pthread_mutex_unlock(&mutex);

	//End
	session->End();

	//Delete sessionerence
	delete session;

	Log("<DeleteSession [%d]\n",id);

	//Exit
	return true;
}

bool Broadcaster::GetBroadcastPublishedStreams(DWORD sessId,BroadcastSession::PublishedStreamsInfo &list)
{
	BroadcastSession *sess;
	//Get ref
	if(!GetBroadcastRef(sessId,&sess))
		//Error
		return Error("Broadcast id not found\n");
	//Get list
	sess->GetBroadcastPublishedStreams(list);
	//Release ref
	ReleaseBroadcastRef(sessId);
	//OK
	return true;
}

RTMPNetConnection* Broadcaster::Connect(const std::wstring& appName,RTMPNetConnection::Listener* listener)
{
	//Depending on the app nam
	if (appName.compare(L"streamer")==0)
	{
		//Create new streamer connection
		return NULL;//new RTMPStreamerNetConnection();

		/*//A receiver stream
		return new RTMPFLVStream(streamId);
		} else if (appName.compare(L"streamer/mp4")==0) {
		Log("-Creating MP4 streamer stream\n");
		//A receiver stream
		return new RTMPMP4Stream(streamId);
		 * */
	} else if (appName.find(L"broadcaster")==0) {
		BroadcastSession* sess = NULL;
		RTMPNetConnection* conn = NULL;
		DWORD sessId = 0;

		//Get the broadcaster type part
		int i = appName.find(L"/",0);

		//Check if not found
		if (i==std::wstring::npos)
			//End
			goto error;

		//Get the token part
		int j = appName.find(L"/",i+1);

		//Check if not found
		if (i==std::wstring::npos)
			//End
			goto error;
		//Get type
		std::wstring type = appName.substr(i+1,j-i-1);
		//Get conf id
		std::wstring val  = appName.substr(j+1);

		Log("-Got connection [type:%ls,val:%ls]\n",type.c_str(),val.c_str());

		//Depending on the type
		if (type.compare(L"publisher")== 0)
		{
			//Get session id from token
			sessId = GetPublishedBroadcastId(val);
			//Get conference if got id
			if(!sessId || !GetBroadcastRef(sessId,&sess))
			{
				//No conference found
				Error("BroadcastSession not found [sessId:%d]\n",sessId);
				//Send error
				listener->onNetConnectionStatus(RTMP::NetConnection::Connect::Closed,L"BroadcastSession not found");
				//Exit
				return NULL;
			}

			//Connect
			conn = sess->ConnectPublisher(listener);

			//release it
			ReleaseBroadcastRef(sessId);
		} else if (type.compare(L"watcher")== 0) {
			//Get session id from token
			sessId = GetTokenBroadcastId(val);
			//Check if found
			if (!sessId)
				//Try the pin
				sessId = GetPublishedBroadcastId(val);
			//Get conference if got id
			if(!sessId || !GetBroadcastRef(sessId,&sess))
			{
				//No conference found
				Error("BroadcastSession not found [sessId:%d]\n",sessId);
				//Send error
				listener->onNetConnectionStatus(RTMP::NetConnection::Connect::Closed,L"BroadcastSession not found");
				//Exit
				return NULL;
			}

			//Connect
			conn = sess->ConnectWatcher(listener);

			//release it
			ReleaseBroadcastRef(sessId);
		}

		//Return conf
		return conn;

	}

error:
	//Send error
	listener->onNetConnectionStatus(RTMP::NetConnection::Connect::InvalidApp,L"Wrong application name");
	
	//Exit
	return NULL;
}

RTMPNetStream* Broadcaster::CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener* listener)
{
	//Create stream
	RTMPNetStream* stream = new NetStream(streamId,listener);
	//Register it
	RegisterStream(stream);
	//return it
	return stream;
}

void Broadcaster::DeleteStream(RTMPNetStream *stream)
{
	//Unregister
	UnRegisterStream(stream);
	//Delete
	delete(stream);
}

Broadcaster::NetStream::NetStream(DWORD id,Listener *listener) : RTMPNetStream(id,listener)
{

}

Broadcaster::NetStream::~NetStream()
{

}
void Broadcaster::NetStream::doPlay(std::wstring& url,RTMPMediaStream::Listener *listener)
{

}
void Broadcaster::NetStream::doPublish(std::wstring& url)
{

}
void Broadcaster::NetStream::doPause()
{
}

void Broadcaster::NetStream::doResume()
{

}

void Broadcaster::NetStream::doSeek(DWORD time)
{

}

void Broadcaster::NetStream::doClose(RTMPMediaStream::Listener *listener)
{

}
