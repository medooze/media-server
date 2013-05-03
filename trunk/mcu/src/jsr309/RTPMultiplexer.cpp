/* 
 * File:   RTPMultiplexer.cpp
 * Author: Sergio
 * 
 * Created on 7 de septiembre de 2011, 12:19
 */
#include "log.h"
#include "RTPMultiplexer.h"

RTPMultiplexer::RTPMultiplexer()
{
	//Create mutex
	pthread_mutex_init(&mutex,NULL);
}

RTPMultiplexer::~RTPMultiplexer()
{
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Remove stream
		(*it)->onEndStream();
	//Clean listeners
	listeners.clear();
	//Unlock
	pthread_mutex_unlock(&mutex);
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

void RTPMultiplexer::Multiplex(RTPPacket &packet)
{
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Update
		(*it)->onRTPPacket(packet);
	//Unlock
	pthread_mutex_unlock(&mutex);
}

void RTPMultiplexer::ResetStream()
{
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Update
		(*it)->onResetStream();
	//Unlock
	pthread_mutex_unlock(&mutex);
}

void RTPMultiplexer::EndStream()
{
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Update
		(*it)->onEndStream();
	//Unlock
	pthread_mutex_unlock(&mutex);
}

void RTPMultiplexer::AddListener(Listener *listener)
{
	//reset it
	listener->onResetStream();
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Apend
	listeners.insert(listener);
	//Unlock
	pthread_mutex_unlock(&mutex);
}

void RTPMultiplexer::RemoveListener(Listener *listener)
{
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Find it
	Listeners::iterator it = listeners.find(listener);
	//If present
	if (it!=listeners.end())
		//erase it
		listeners.erase(it);
	//Unlock
	pthread_mutex_unlock(&mutex);
}

void RTPMultiplexer::Update()
{
	//Should be overriden
}

void RTPMultiplexer::SetREMB(DWORD estimation)
{
	//Should be overriden
}