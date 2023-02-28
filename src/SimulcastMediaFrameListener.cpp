#include "SimulcastMediaFrameListener.h"
#include "video.h"

SimulcastMediaFrameListener::SimulcastMediaFrameListener(TimeService& timeService, DWORD ssrc, DWORD numLayers) :
	timeService(timeService),
	ssrc(ssrc)
{
}

SimulcastMediaFrameListener::~SimulcastMediaFrameListener()
{
}

void SimulcastMediaFrameListener::AddMediaListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-MediaFrameListenerBridge::AddListener() [this:%p,listener:%p]\n", this, listener.get());
	
	timeService.Sync([=](std::chrono::milliseconds){
		listeners.insert(listener);
	});
}

void SimulcastMediaFrameListener::RemoveMediaListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-MediaFrameListenerBridge::RemoveListener() [this:%p,listener:%p]\n", this, listener.get());
	timeService.Sync([=](std::chrono::milliseconds){
		listeners.erase(listener);
	});
}

void SimulcastMediaFrameListener::Stop()
{
	timeService.Sync([=](std::chrono::milliseconds) {
		// Store remaining
		selector.Flush([this](VideoFrame& frame){
			ForwardFrame(frame);
		});

		//Clear listeners
		listeners.clear();
	});
}

void SimulcastMediaFrameListener::SetNumLayers(DWORD numLayers)
{
}


void SimulcastMediaFrameListener::onMediaFrame(DWORD ssrc, const MediaFrame& frame)
{
	//Ensure it is video
	if (frame.GetType()!=MediaFrame::Video)
		//Uh?
		return;

	//Get cloned video frame
	auto cloned = std::unique_ptr<VideoFrame>((VideoFrame*)frame.Clone());
	cloned->SetSSRC(ssrc);

	selector.Push(std::move(cloned), [this](VideoFrame& frame){
		ForwardFrame(frame);
	});
}

void SimulcastMediaFrameListener::ForwardFrame(VideoFrame& frame)
{
	//Debug
	//UltraDebug("-SimulcastMediaFrameListener::ForwardFrame() | Forwarding frame [ts:%llu]\n", frame.GetTimestamp());

	//Send it to all listeners
	for (const auto& listener : listeners)
		//Send cloned frame
		listener->onMediaFrame(ssrc, frame);
}
