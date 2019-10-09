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
	for(auto it = listeners.begin(); it!=listeners.end(); ++it)
		//Disconnect
		(*it)->onNetConnectionStatus(info,message);
	//Unlock
	lock.Unlock();
}
	
void RTMPNetConnection::Disconnect()
{
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Clean streams
	streams.clear();
	//For each listener
	for(auto it = listeners.begin(); it!=listeners.end(); ++it)
		//Disconnect
		(*it)->onNetConnectionDisconnected();
	//Unlock
	lock.Unlock();
}

int RTMPNetConnection::RegisterStream(const RTMPNetStream::shared& stream)
{
	Log(">RTMPNetConnection::RegisterStream() [tag:%ls]\n",stream->GetTag().c_str());

	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Apend
	streams[stream->GetStreamId()] = stream;
	//Get number of streams
	DWORD num = streams.size();
	//Unlock
	lock.Unlock();

	Log("<RTMPNetConnection::RegisterStream() [size:%d]\n",num);
	
	//return number of streams
	return num;
}

int RTMPNetConnection::UnRegisterStream(const RTMPNetStream::shared& stream)
{

	Log(">RTMPNetConnection::UnRegisterStream() [tag:%ls]\n",stream->GetTag().c_str());

	//Lock mutexk
	lock.WaitUnusedAndLock();
	//erase it
	streams.erase(stream->GetStreamId());
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
	auto it = listeners.find(listener);
	//If present
	if (it!=listeners.end())
		//erase it
		listeners.erase(it);
	//Unlock
	lock.Unlock();
	
}
