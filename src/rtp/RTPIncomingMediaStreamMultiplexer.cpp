#include "tracing.h"

#include "rtp/RTPIncomingMediaStreamMultiplexer.h"

	
RTPIncomingMediaStreamMultiplexer::RTPIncomingMediaStreamMultiplexer(const RTPIncomingMediaStream::shared& incomingMediaStream,TimeService& timeService) :
	incomingMediaStream(incomingMediaStream),
	timeService(timeService)
{

	Debug("-RTPIncomingMediaStreamMultiplexer::RTPIncomingMediaStreamMultiplexer() [stream:%p,this:%p]\n", incomingMediaStream, this);

	if (incomingMediaStream)
	{	
		//Add us as listeners
		incomingMediaStream->AddListener(this);
	}
}

void RTPIncomingMediaStreamMultiplexer::Stop()
{
	Debug("-RTPIncomingMediaStreamMultiplexer::Stop() [this:%p]\n", this);

	//Wait until all the previous async have finished as async calls are executed in order
	timeService.Sync([=](auto now){
		//Trace method
		TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::Stop async", "ssrc", GetMediaSSRC());
		//If the source stream is alive
		if (incomingMediaStream)
			//Do not listen anymore
			incomingMediaStream->RemoveListener(this);
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onEnded(this);
		//Remove all listeners
		listeners.clear();
	});
}

void RTPIncomingMediaStreamMultiplexer::AddListener(RTPIncomingMediaStream::Listener* listener)
{
	Debug("-RTPIncomingMediaStreamMultiplexer::AddListener() [listener:%p,this:%p]\n",listener,this);
	
	//Dispatch in thread sync
	timeService.Async([=](auto now){
		listeners.insert(listener);
	});
}

void RTPIncomingMediaStreamMultiplexer::RemoveListener(RTPIncomingMediaStream::Listener* listener)
{
	Debug("-RTPIncomingMediaStreamMultiplexer::RemoveListener() [listener:%p,this:%p]\n", listener, this);
		
	//Dispatch in thread sync
	timeService.Sync([=](auto now){
		listeners.erase(listener);
	});
}

void RTPIncomingMediaStreamMultiplexer::onRTP(const RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onRTP", "ssrc", stream->GetMediaSSRC());

	//If not muted
	if (!muted)
	{
		//Dispatch in thread async
		timeService.Async([=](auto now){
			//Deliver to all listeners
			for (auto listener : listeners)
				//Dispatch rtp packet
				listener->onRTP(this,packet);
		});
	}
}

void RTPIncomingMediaStreamMultiplexer::onRTP(const RTPIncomingMediaStream* stream,const std::vector<RTPPacket::shared>& packets)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onRTP", "ssrc", stream->GetMediaSSRC(), "packets", packets.size());

	//If not muted
	if (!muted)
	{
		//Dispatch in thread async
		timeService.Async([=,ssrc = stream->GetMediaSSRC()](auto now){
			//Trace method
			TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onRTP async", "ssrc", ssrc, "packets", packets.size());
			//Process each packet in order, if we reverse the order of the loops, the last listeners would have a lot of delay
			for (const auto& packet : packets)
				//Deliver to all listeners
				for (auto listener : listeners)
					//Dispatch rtp packet
					listener->onRTP(this,packet);
		});
	}
}


void RTPIncomingMediaStreamMultiplexer::onBye(const RTPIncomingMediaStream* stream)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onBye", "ssrc", stream->GetMediaSSRC());

	//Dispatch in thread async
	timeService.Async([=, ssrc = stream->GetMediaSSRC()](auto now){
		//Trace method
		TRACE_EVENT("rtp", "RTPIncomingMediaStreamMultiplexer::onBye async", "ssrc", ssrc);
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onBye(this);
	});
}


void RTPIncomingMediaStreamMultiplexer::onEnded(const RTPIncomingMediaStream* stream)
{
	Debug("-RTPIncomingMediaStreamMultiplexer::onEnded() [stream:%p,this:%p]\n", stream, this);

	//Dispatch in thread sync
	timeService.Sync([=](auto now) {
		//Check
		if (incomingMediaStream.get() == stream)
			//No stream
			incomingMediaStream.reset();
	});
}

void RTPIncomingMediaStreamMultiplexer::Mute(bool muting)
{
	//Log
	UltraDebug("-RTPIncomingMediaStreamMultiplexer::Mute() | [muting:%d]\n", muting);

	//Update state
	muted = muting;
}
