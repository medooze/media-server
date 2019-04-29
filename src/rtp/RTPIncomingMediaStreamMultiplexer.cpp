#include "rtp/RTPIncomingMediaStreamMultiplexer.h"

	
RTPIncomingMediaStreamMultiplexer::RTPIncomingMediaStreamMultiplexer(DWORD ssrc,TimeService& timeService) :
	timeService(timeService)
{
	//Store type
	this->ssrc = ssrc;
}


void RTPIncomingMediaStreamMultiplexer::AddListener(RTPIncomingMediaStream::Listener* listener) 
{
	Debug("-RTPIncomingMediaStreamMultiplexer::AddListener() [listener:%p,this:%p]\n",listener,this);
		
	ScopedLock scoped(listenerMutex);
	listeners.insert(listener);
}

void RTPIncomingMediaStreamMultiplexer::RemoveListener(RTPIncomingMediaStream::Listener* listener) 
{
	Debug("-RTPIncomingMediaStreamMultiplexer::RemoveListener() [listener:%p]\n",listener);
		
	ScopedLock scoped(listenerMutex);
	listeners.erase(listener);
}

void RTPIncomingMediaStreamMultiplexer::onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Dispatch in thread async
	timeService.Async([=](...){
		//Block listeners
		ScopedLock scoped(listenerMutex);
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onRTP(this,packet);
	});
}

void RTPIncomingMediaStreamMultiplexer::onRTP(RTPIncomingMediaStream* stream,const std::vector<RTPPacket::shared>& packets)
{
	//Dispatch in thread async
	timeService.Async([=](...){
		//Block listeners
		ScopedLock scoped(listenerMutex);
		//For each packet
		for (const auto packet : packets)
			//Deliver to all listeners
			for (auto listener : listeners)
				//Dispatch rtp packet
				listener->onRTP(stream,packet);
	});
}



void RTPIncomingMediaStreamMultiplexer::onEnded(RTPIncomingMediaStream* stream)
{
	//Dispatch in thread async
	timeService.Async([=](...){
		//Block listeners
		ScopedLock scoped(listenerMutex);
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onEnded(this);
	});
}
