#include "tracing.h"

#include "rtp/RTPIncomingMediaStreamMultiplexer.h"

	
RTPIncomingMediaStreamMultiplexer::RTPIncomingMediaStreamMultiplexer(DWORD ssrc,TimeService& timeService) :
	timeService(timeService)
{
	//Store type
	this->ssrc = ssrc;
}

void RTPIncomingMediaStreamMultiplexer::Stop()
{
	//Wait until all the previous async have finished as async calls are executed in order
	timeService.Async([=](...){}).wait();
}

void RTPIncomingMediaStreamMultiplexer::AddListener(RTPIncomingMediaStream::Listener* listener) 
{
	Debug("-RTPIncomingMediaStreamMultiplexer::AddListener() [listener:%p,this:%p]\n",listener,this);
	
	//Dispatch in thread sync
	timeService.Sync([=](...){
		listeners.insert(listener);
	});
}

void RTPIncomingMediaStreamMultiplexer::RemoveListener(RTPIncomingMediaStream::Listener* listener) 
{
	Debug("-RTPIncomingMediaStreamMultiplexer::RemoveListener() [listener:%p,this:%p]\n", listener, this);
		
	//Dispatch in thread sync
	timeService.Sync([=](...){
		listeners.erase(listener);
	});
}

void RTPIncomingMediaStreamMultiplexer::onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onRTP", "ssrc", stream->GetMediaSSRC());

	//Dispatch in thread async
	timeService.Async([=](...){
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onRTP(this,packet);
	});
}

void RTPIncomingMediaStreamMultiplexer::onRTP(RTPIncomingMediaStream* stream,const std::vector<RTPPacket::shared>& packets)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onRTP", "ssrc", stream->GetMediaSSRC(), "packets", packets.size());

	//Dispatch in thread async
	timeService.Async([=,ssrc = stream->GetMediaSSRC()](...){
		//Trace method
		TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onRTP async", "ssrc", ssrc, "packets", packets.size());
		//For each packet
		for (const auto& packet : packets)
			//Deliver to all listeners
			for (auto listener : listeners)
				//Dispatch rtp packet
				listener->onRTP(this,packet);
	});
}


void RTPIncomingMediaStreamMultiplexer::onBye(RTPIncomingMediaStream* stream)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onBye", "ssrc", stream->GetMediaSSRC());

	//Dispatch in thread async
	timeService.Async([=, ssrc = stream->GetMediaSSRC()](...){
		//Trace method
		TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onBye async", "ssrc", ssrc);
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onBye(this);
	});
}


void RTPIncomingMediaStreamMultiplexer::onEnded(RTPIncomingMediaStream* stream)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onEnded", "ssrc", stream->GetMediaSSRC());

	//Dispatch in thread async
	timeService.Async([=](...){
		//Trace method
		TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onEnded async", "ssrc", ssrc);
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onEnded(this);
	});
}
