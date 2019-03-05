#include "rtmp/rtmpnetconnection.h"

RTMPNetConnection::~RTMPNetConnection()
{
	//Disconnected
	Disconnect();
}


void RTMPNetConnection::SendStatus(const RTMPNetStatusEventInfo &info,const wchar_t *message)
{
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//For each listener
	for(Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Disconnect
		(*it)->onNetConnectionStatus(info,message);
	//Unlock
	lock.Unlock();
}
	
void RTMPNetConnection::Disconnect()
{
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//For each stream
	for(RTMPNetStreams::iterator it = streams.begin(); it!=streams.end(); ++it)
		//Delete it
		delete(*it);
	//Clean streams
	streams.clear();
	//For each listener
	for(Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Disconnect
		(*it)->onNetConnectionDisconnected();
	//Unlock
	lock.Unlock();
}

int RTMPNetConnection::RegisterStream(RTMPNetStream* stream)
{
	Log(">RTMPNetConnection::RegisterStream() [tag:%ls]\n",stream->GetTag().c_str());

	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Apend
	streams.insert(stream);
	//Get number of streams
	DWORD num = streams.size();
	//Unlock
	lock.Unlock();

	Log("<RTMPNetConnection::RegisterStream() [size:%d]\n",num);
	
	//return number of streams
	return num;
}

int RTMPNetConnection::UnRegisterStream(RTMPNetStream* stream)
{

	Log(">RTMPNetConnection::UnRegisterStream() [tag:%ls]\n",stream->GetTag().c_str());

	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Find it
	RTMPNetStreams::iterator it = streams.find(stream);
	//If present
	if (it!=streams.end())
		//erase it
		streams.erase(it);
	//Get number of streams
	DWORD num = streams.size();
	//Unlock
	lock.Unlock();

	Log("<RTMPNetConnection::UnRegisterStream()[size:%d]\n",num);

	//return number of streams
	return num;
}

void RTMPNetConnection::AddListener(Listener* listener)
{
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Apend
	listeners.insert(listener);
	//Unlock
	lock.Unlock();
}


void RTMPNetConnection::RemoveListener(Listener* listener)
{
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Find it
	Listeners::iterator it = listeners.find(listener);
	//If present
	if (it!=listeners.end())
		//erase it
		listeners.erase(it);
	//Unlock
	lock.Unlock();
	
}
