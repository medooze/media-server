#include "rtp/RTPIncomingMediaStreamDepacketizer.h"
#include "use.h"

RTPIncomingMediaStreamDepacketizer::RTPIncomingMediaStreamDepacketizer(RTPIncomingMediaStream* incomingSource)
{
	Debug("-RTPIncomingMediaStreamDepacketizer::RTPIncomingMediaStreamDepacketizer() [%p]\n",incomingSource);
	
	//Store
	this->incomingSource = incomingSource;
	//Add us as RTP listeners
	this->incomingSource->AddListener(this);
	//No depkacketixer yet
	depacketizer = NULL;
}

RTPIncomingMediaStreamDepacketizer::~RTPIncomingMediaStreamDepacketizer()
{
	Debug("-RTPIncomingMediaStreamDepacketizer::~RTPIncomingMediaStreamDepacketizer()\n");
	
	//JIC
	Stop();
	//Check 
	if (depacketizer)
		//Delete depacketier
		delete(depacketizer);
}

void RTPIncomingMediaStreamDepacketizer::onRTP(RTPIncomingMediaStream* group,const RTPPacket::shared& packet)
{
	//Do not do extra work if there are no listeners
	if (listeners.empty()) 
		return;

	//If depacketizer is not the same codec 
	if (depacketizer && depacketizer->GetCodec()!=packet->GetCodec())
	{
		//Delete it
		delete(depacketizer);
		//Create it next
		depacketizer = NULL;
	}
	//If we don't have a depacketized
	if (!depacketizer)
		//Create one
		depacketizer = RTPDepacketizer::Create(packet->GetMedia(),packet->GetCodec());
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

void RTPIncomingMediaStreamDepacketizer::onBye(RTPIncomingMediaStream* group) 
{
	Debug("-RTPIncomingMediaStreamDepacketizer::onBye() [group:%p]\n", group);
	
	if(depacketizer)
		//Skip current
		depacketizer->ResetFrame();
}

void RTPIncomingMediaStreamDepacketizer::onEnded(RTPIncomingMediaStream* group) 
{
	ScopedLock scoped(mutex);
	
	//Lock	
	Debug("-RTPIncomingMediaStreamDepacketizer::onEnded() [group:%p]\n", group);
	
	//Check it is ours
	if (incomingSource==group)
		incomingSource = nullptr;
}

void RTPIncomingMediaStreamDepacketizer::AddMediaListener(MediaFrame::Listener *listener)
{
	ScopedLock scoped(mutex);
	
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::AddMediaListener() [listener:%p]\n", listener);
	
	//Check listener
	if (!listener)
		//Done
		return;
	
	//Add listener async
	incomingSource->GetTimeService().Async([=](...){
		//Add to set
		listeners.insert(listener);
	});
}

void RTPIncomingMediaStreamDepacketizer::RemoveMediaListener(MediaFrame::Listener *listener)
{
	ScopedLock scoped(mutex);
	
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::RemoveMediaListener() [listener:%p]\n", listener);
	
	//Check listener
	if (!incomingSource || !listener)
		//Done
		return;

	//Add listener sync so it can be deleted after this call
	incomingSource->GetTimeService().Sync([=](...){
		//Remove from set
		listeners.erase(listener);
	});
}

void RTPIncomingMediaStreamDepacketizer::Stop()
{
	ScopedLock scoped(mutex);
	
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::Stop() [%p]\n",incomingSource);
	
	//If already stopped
	if (!incomingSource)
		//Done
		return;
		
	//Stop listeneing
	incomingSource->RemoveListener(this);
	//Clean it
	incomingSource = NULL;
}
