#include "rtp/RTPIncomingMediaStreamDepacketizer.h"
#include "use.h"

RTPIncomingMediaStreamDepacketizer::RTPIncomingMediaStreamDepacketizer(RTPIncomingMediaStream* incomingSource) :
	timeService(incomingSource->GetTimeService())
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
	
	//If not already stopped
	if (!incomingSource)
		//Stop it
		Stop();
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
	//Lock	
	Debug("-RTPIncomingMediaStreamDepacketizer::onEnded() [group:%p]\n", group);
	
	//Check it is ours
	if (incomingSource==group)
		incomingSource = nullptr;
}

void RTPIncomingMediaStreamDepacketizer::AddMediaListener(MediaFrame::Listener *listener)
{
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::AddMediaListener() [listener:%p]\n", listener);
	
	//Check listener
	if (!listener)
		//Done
		return;

	//Add listener Sync
	timeService.Sync([=](...){
		//Add to set
		listeners.insert(listener);
	});
}

void RTPIncomingMediaStreamDepacketizer::RemoveMediaListener(MediaFrame::Listener *listener)
{
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::RemoveMediaListener() [listener:%p]\n", listener);
	
	//Add listener sync so it can be deleted after this call
	timeService.Sync([=](...){
		//Check listener
		if (!incomingSource || !listener)
			//Done
			return;
		//Remove from set
		listeners.erase(listener);
	});
}

void RTPIncomingMediaStreamDepacketizer::Stop()
{
	//Log
	Debug("-RTPIncomingMediaStreamDepacketizer::Stop() [%p]\n",incomingSource);

	timeService.Sync([=](...){
		//If already stopped
		if (!incomingSource)
			//Done
			return;
		//Stop listeneing
		incomingSource->RemoveListener(this);
		//Clean it
		incomingSource = NULL;
		//Check 
		if (depacketizer)
			//Delete depacketier
			delete(depacketizer);
	});
}
