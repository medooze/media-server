#include "rtmpnetconnection.h"

RTMPNetConnection::~RTMPNetConnection()
{
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//For each stream
	for(RTMPNetStreams::iterator it = streams.begin(); it!=streams.end(); ++it)
		//Delete it
		delete(*it);
	//Clean streams
	streams.clear();
	//Disconnected
	fireOnNetConnectionDisconnected();
	//Clean streams
	listeners.clear();
	//Unlock
	lock.Unlock();
}

void RTMPNetConnection::fireOnNetConnectionDisconnected()
{
	//For each listener
	for(Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Disconnect
		(*it)->onNetConnectionDisconnected();
}

int RTMPNetConnection::RegisterStream(RTMPNetStream* stream)
{
	Log(">Registering stream [tag:%ls]\n",stream->GetTag().c_str());

	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Apend
	streams.insert(stream);
	//Get number of streams
	DWORD num = streams.size();
	//Unlock
	lock.Unlock();

	Log("<Unregistering string [num:%d]\n",num);
	
	//return number of streams
	return num;
}

int RTMPNetConnection::UnRegisterStream(RTMPNetStream* stream)
{

	Log(">Unregistering string [tag:%ls]\n",stream->GetTag().c_str());

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

	Log("<Unregistering string [num:%d]\n",num);

	//return number of streams
	return num;
}

void RTMPNetConnection::Connect(Listener* listener)
{
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Apend
	listeners.insert(listener);
	//Unlock
	lock.Unlock();
}


void RTMPNetConnection::Disconnect(Listener* listener)
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
