#include "rtmp/rtmpnetconnection.h"

RTMPNetConnection::~RTMPNetConnection()
{
	//Disconnected
	Disconnect();
}


void RTMPNetConnection::SendStatus(QWORD transId, const RTMPNetStatusEventInfo &info,const wchar_t *message)
{
	//Lock mutexk
	listenerLock.Lock();
	//For each listener
	for(auto it = listeners.begin(); it!=listeners.end(); ++it)
		//Disconnect
		(*it)->onNetConnectionStatus(transId, info,message);
	//Unlock
	listenerLock.Unlock();
}
	
void RTMPNetConnection::Disconnect()
{
	//Lock mutex
	streamLock.Lock();
	//Clean streams
	streams.clear();
	//Unlock
	streamLock.Unlock();
	
	//Lock listeners
	listenerLock.Lock();
	//For each listener
	for(auto it = listeners.begin(); it!=listeners.end(); ++it)
		//Disconnect
		(*it)->onNetConnectionDisconnected();
	//Remove all listeners
	listeners.clear();
	//Unlock
	listenerLock.Unlock();
}

void RTMPNetConnection::Disconnected()
{
}

int RTMPNetConnection::RegisterStream(const RTMPNetStream::shared& stream)
{
	Log(">RTMPNetConnection::RegisterStream() [tag:%ls]\n",stream->GetTag().c_str());

	//Lock mutexk
	streamLock.Lock();
	//Apend
	streams[stream->GetStreamId()] = stream;
	//Get number of streams
	DWORD num = streams.size();
	//Unlock
	streamLock.Unlock();

	Log("<RTMPNetConnection::RegisterStream() [size:%d]\n",num);
	
	//return number of streams
	return num;
}

int RTMPNetConnection::UnRegisterStream(const RTMPNetStream::shared& stream)
{

	Log(">RTMPNetConnection::UnRegisterStream() [tag:%ls]\n",stream->GetTag().c_str());

	//Lock mutexk
	streamLock.Lock();
	//erase it
	streams.erase(stream->GetStreamId());
	//Get number of streams
	DWORD num = streams.size();
	//Unlock
	streamLock.Unlock();

	Log("<RTMPNetConnection::UnRegisterStream()[size:%d]\n",num);

	//return number of streams
	return num;
}

void RTMPNetConnection::AddListener(Listener* listener)
{
	//Lock mutexk
	listenerLock.Lock();
	//Apend
	listeners.insert(listener);
	//Unlock
	listenerLock.Unlock();
}


void RTMPNetConnection::RemoveListener(Listener* listener)
{
	//Lock mutexk
	listenerLock.Lock();
	//erase it
	listeners.erase(listener);
	//Unlock
	listenerLock.Unlock();
	
}
