#include "rtp/RTPIncomingMediaStreamDepacketizer.h"
#include "use.h"

RTPIncomingMediaStreamDepacketizer::RTPIncomingMediaStreamDepacketizer(const RTPIncomingMediaStream::shared& incomingSource) :
	timeService(incomingSource->GetTimeService())
{
	Debug("-RTPIncomingMediaStreamDepacketizer::RTPIncomingMediaStreamDepacketizer() [this:%p,incomingSource:%p]\n", this, incomingSource.get());
	
	//Store
	this->incomingSource = incomingSource;
	//Add us as RTP listeners
	this->incomingSource->AddListener(this);
}

RTPIncomingMediaStreamDepacketizer::~RTPIncomingMediaStreamDepacketizer()
{
	Debug("-RTPIncomingMediaStreamDepacketizer::~RTPIncomingMediaStreamDepacketizer() [this:%p,incomingSource:%p]\n", this, incomingSource.get());

}

void RTPIncomingMediaStreamDepacketizer::onRTP(const RTPIncomingMediaStream* group,const RTPPacket::shared& packet)
{
	//Do not do extra work if there are no listeners
	if (listeners.empty()) 
		return;

	//If we don't have a depacketized or is not the same codec 
	if (!depacketizer || depacketizer->GetCodec()!=packet->GetCodec())
		//Create one
		depacketizer.reset(RTPDepacketizer::Create(packet->GetMedia(),packet->GetCodec()));

	//Ensure we have it
	if (!depacketizer)
		//Do nothing
		return;

	//Pass the pakcet to the depacketizer
	 MediaFrame* frame = depacketizer->AddPacket(packet);

	 //If we have a new frame
	 if (frame)
	 {
		 //Call all listeners
		 for (const auto& listener : listeners)
			 //Call listener
			 listener->onMediaFrame(packet->GetSSRC(),*frame);
		 //Next
		 depacketizer->ResetFrame();
	 }
}

void RTPIncomingMediaStreamDepacketizer::onBye(const RTPIncomingMediaStream* group)
{
	Debug("-RTPIncomingMediaStreamDepacketizer::onBye() [this:%p,group:%p]\n", this, group);
	//Check it is ours
	if (incomingSource.get() == group && depacketizer)
		//Skip current
		depacketizer->ResetFrame();
}

void RTPIncomingMediaStreamDepacketizer::onEnded(const RTPIncomingMediaStream* group)
{
	//Lock	
	Debug("-RTPIncomingMediaStreamDepacketizer::onEnded() [this:%p,group:%p]\n", this, group);
	
	//Check it is ours
	if (incomingSource.get()==group)
		//Remove source
		incomingSource = nullptr;
}

void RTPIncomingMediaStreamDepacketizer::AddMediaListener(const MediaFrame::Listener::shared& listener)
{
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::AddMediaListener() [this:%p,listener:%p]\n", this, listener.get());
	
	//Check listener
	if (!listener)
		//Done
		return;

	//Add listener Sync
	timeService.Async([=](auto now){
		//Add to set
		listeners.insert(listener);
	});
}

void RTPIncomingMediaStreamDepacketizer::RemoveMediaListener(const MediaFrame::Listener::shared& listener)
{
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::RemoveMediaListener() [this:%p,listener:%p]\n", this, listener.get());
	
	//Add listener sync so it can be deleted after this call
	timeService.Sync([=](auto now){
		//Check listener
		if (!listener)
			//Done
			return;
		//Remove from set
		listeners.erase(listener);
	});
}

void RTPIncomingMediaStreamDepacketizer::Stop()
{
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::Stop() [this:%p,incomingSource:%p]\n", this, incomingSource.get());

	timeService.Sync([=](auto now){
		//If still have a incoming source
		if (incomingSource)
		{
			//Stop listeneing
			incomingSource->RemoveListener(this);
			//Clean it
			incomingSource.reset();
		}
	});
}
