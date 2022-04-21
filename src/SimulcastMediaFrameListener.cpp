#include "SimulcastMediaFrameListener.h"


SimulcastMediaFrameListener::SimulcastMediaFrameListener(DWORD ssrc, DWORD numLayers) :
	ssrc(ssrc),
	numLayers(numLayers)
{
}

SimulcastMediaFrameListener::~SimulcastMediaFrameListener()
{
	Select();
}

void SimulcastMediaFrameListener::AddMediaListener(MediaFrame::Listener* listener)
{
	Debug("-MediaFrameListenerBridge::AddListener() [listener:%p]\n", listener);
	ScopedLock lock(mutex);
	listeners.insert(listener);
}

void SimulcastMediaFrameListener::RemoveMediaListener(MediaFrame::Listener* listener)
{
	Debug("-MediaFrameListenerBridge::RemoveListener() [listener:%p]\n", listener);
	ScopedLock lock(mutex);
	listeners.erase(listener);
}

void SimulcastMediaFrameListener::Stop()
{
	//Select last frame
	Select();
	//Lock
	ScopedLock lock(mutex);
	//Clear listeners
	listeners.clear();
}

void SimulcastMediaFrameListener::SetNumLayers(DWORD numLayers)
{
	this->numLayers = numLayers;
}

void SimulcastMediaFrameListener::Select()
{
	//UltraDebug("-MediaFrameListenerBridge::Select() | [iframes:%u,pending:%d,forwarded:%u]\n", iframes.size(), pendingFrames.size(), forwarded);

	DWORD prev = forwarded;
	VideoFrame* selected = nullptr;
	DWORD currentDimensions = 0;
	DWORD currentSize = 0;

	//For all pending frames
	for (const auto& [ssrc, videoFrame] : iframes)
	{

		//UltraDebug("-MediaFrameListenerBridge::Select() | candidate [ssrc:%u,size:%d,dimensions:%ux%u,ts:%llu]\n", ssrc, videoFrame->GetLength(), videoFrame->GetWidth() , videoFrame->GetHeight(), videoFrame->GetTimeStamp());
		//Calculate dimensions
		DWORD dimensions = videoFrame->GetWidth() * videoFrame->GetHeight();
		//Check if it is bigger, bigger is better
		if (currentDimensions<dimensions && currentSize<videoFrame->GetLength())
		{
			//Try this
			selected = videoFrame.get();
			forwarded = ssrc;
			currentSize = videoFrame->GetLength();
		}
	}

	//If we found none
	if (!selected)
		return;

	//If we are changing
	if (prev!=forwarded)
	{
		//Recaulcate timestamp offsets diff
		uint64_t diff = 0;

		//Check if We have an iframe of the previus selected simulcast layer
		auto it = iframes.find(prev);

		//If found
		if (it != iframes.end())
		{
			//Ensure it was increasing
			if (it->second->GetTimeStamp() > lastTimestamp)
				//Get timestamp difference between both frames on previous layer
				diff = it->second->GetTimeStamp() - lastTimestamp;
		} else {
			//Ensure it is increasing
			if (selected->GetTime() > lastTime)
				//Get the timestamp difference between the time difference of this frame and the previous sent one
				diff = (selected->GetTime() - lastTime) * selected->GetClockRate() / 1000 ;
		}

		//Increment offset
		offsetTimestamp += (lastTimestamp - firstTimestamp) + diff;

		//Reset first timestamp
		firstTimestamp = selected->GetTimestamp();

		//Debug
		//UltraDebug("-MediaFrameListenerBridge::Select() | Changing active layer [prev:%u,current:%u,offset:%llu]\n", prev, forwarded, offsetTimestamp);
	}

	//Forward frame
	ForwardFrame(*selected);

	//Clear pending
	iframes.clear();

	//Not selecting
	selectionTime = 0;

	//For all pending frames
	for ( auto& [ssrc,pending] : pendingFrames)
		//If the frame is from the forwarded simulcast layer
		if (ssrc == forwarded)
			//Forward frame
			ForwardFrame(*pending);

	//Clear pending frames
	pendingFrames.clear();
}

void SimulcastMediaFrameListener::onMediaFrame(DWORD ssrc, const MediaFrame& frame)
{
	//Ensure it is video
	if (frame.GetType()!=MediaFrame::Video)
		//Uh?
		return;

	//Get frame time
	uint64_t frameTime = frame.GetTime();

	//Get cloned video frame
	auto cloned = std::unique_ptr<VideoFrame>((VideoFrame*)frame.Clone());


	//UltraDebug("-SimulcastMediaFrameListener::onMediaFrame() [ssrc:%u:,intra:%d,iframes:%d,pending:%d,ts:%llu,frameTime:%llu,selectionTime:%llu]\n", ssrc, cloned->IsIntra(), iframes.size(), pendingFrames.size(), cloned->GetTimestamp(), frameTime, selectionTime);


	//We decide which layer to forwrd on each I frame
	if (cloned->IsIntra())
	{
		//If it is the first one
		if (iframes.empty())
			//Start selection from now
			selectionTime = frame.GetTime();
		//Move cloned frame to the pending frames for selection
		auto res = iframes.try_emplace(ssrc, std::move(cloned));
		//If there was already another one for that layer
		if (!res.second)
			//Buffer it
			pendingFrames.push_back({ ssrc, std::move(cloned) });
		//If we have enough I frames or we timed out waiting
		if (iframes.size() == numLayers || (frameTime > selectionTime + 100))
			//Do simulcast layer selection
			Select();
		//Done
		return;
	}

	//Check if we are selecting
	if (selectionTime)
	{
		//Buffer it
		pendingFrames.push_back({ ssrc, std::move(cloned) });

		// If we have enough timed out waiting
		if (frameTime > selectionTime + 100)
			//Don't wait for other layers and force layer selection
			Select();

		//Done
		return;
	}
	
	//If the frame is from the forwarded simulcast layer
	if (ssrc == forwarded)
		//Forward frame
		ForwardFrame(*cloned);
}

void SimulcastMediaFrameListener::ForwardFrame(VideoFrame& frame)
{
	
	//Get timestamp
	uint64_t ts = frame.GetTimestamp();

	//Discard out of order frames
	if (ts < firstTimestamp)
	{
		//Error
		Warning("SimulcastMediaFrameListener::ForwardFrame() | Discarding out of order frame [ts:%llu,first:%llu]\n", ts, firstTimestamp);
		return;
	}

	//Correct timestamp
	frame.SetTimestamp(ts - firstTimestamp + offsetTimestamp);

	//Debug
	//UltraDebug("-SimulcastMediaFrameListener::ForwardFrame() | Forwarding frame [ts:%llu]\n", frame.GetTimestamp());
	ScopedLock lock(mutex);
	//Send it to all listeners
	for (const auto listener : listeners)
		//Send cloned frame
		listener->onMediaFrame(this->ssrc, frame);

	//Set last sent timestamp
	lastTimestamp = ts;
	lastTime = frame.GetTime();
}