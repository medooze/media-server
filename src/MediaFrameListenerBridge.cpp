#include <vector>

#include "video.h"
#include "audio.h"
#include "MediaFrameListenerBridge.h"
#include "VideoLayerSelector.h"

using namespace std::chrono_literals;

MediaFrameListenerBridge::MediaFrameListenerBridge(TimeService& timeService,DWORD ssrc, bool smooth) : 
	timeService(timeService),
	ssrc(ssrc),
	smooth(smooth),
	acumulator(1000),
	accumulatorFrames(1000),
	accumulatorPackets(1000),
	accumulatorIFrames(1000),
	accumulatorBFrames(1000),
	accumulatorPFrames(1000),
	waited(1000)
{
	Debug("-MediaFrameListenerBridge::MediaFrameListenerBridge() [this:%p]\n", this);

	//Create packet dispatch timer
	dispatchTimer = timeService.CreateTimer([this](auto now){

		if (stopped) return;
		
		//Get how much should we send
		auto period = lastSent!=0ms && now >= lastSent ? now - lastSent : 10ms;

		//Updated last dispatched 
		lastSent = now;

		//Packets to send during the period
		std::vector<RTPPacket::shared> sending;

		//Amount of time for the packets
		auto accumulated = 0ms;

		//Until no mor pacekts or full period
		while (packets.size() && period >= accumulated)
		{
			const auto& packetInfo = packets.front();
			if (packetInfo.scheduled > now) break;
			
			//Increase accumulated time
			accumulated += packetInfo.duration;
			//Add to sending packets
			sending.push_back(packetInfo.packet);
			//remove it from pending packets
			packets.pop();
		}

		//UltraDebug("-MediaFrameListenerBridge::dispatchTimer() [queue:%d,sending:%d,period:%llu,accumulated:%llu,next:%d,smooth:%d]\n", packets.size(), sending.size(), period.count(), accumulated.count(), packets.size() && accumulated > period ? (accumulated - period).count(): -1, this->smooth);

		//Dispatch RTP packets
		Dispatch(sending);

		//If we have packets in the queue
		if (packets.size())
		{
			//Reschedule
			auto deferMs = packets.front().scheduled - now;
			if (deferMs.count() > 0)
			{
				dispatchTimer->Again(deferMs);
			}
			else if (accumulated > period)
			{
				dispatchTimer->Again(std::chrono::milliseconds(accumulated - period));
			}
		}
	});
	dispatchTimer->SetName("MediaFrameListenerBridge::dispatchTimer");
}

MediaFrameListenerBridge::~MediaFrameListenerBridge()
{
	Debug("-MediaFrameListenerBridge::~MediaFrameListenerBridge() [this:%p]\n", this);
	
	Stop();
}

void MediaFrameListenerBridge::Stop()
{
	Debug("-MediaFrameListenerBridge::Stop() [this:%p]\n", this);
	
	if (stopped) return;

	timeService.Sync([=](auto now) {
		
		//Dispatch pending READY packets now
		std::vector<RTPPacket::shared> pending;		
		while (packets.size())
		{
			const auto& packetInfo = packets.front();
			
			//Add to pending packets
			pending.push_back(packetInfo.packet);
			//remove it from pending packets
			packets.pop();
		}
		//Dispatch RTP packets
		Dispatch(pending);

                //Updated last dispatched 
		lastSent = now;

		//Cancel dispatch timer
                dispatchTimer->Cancel();
		
		//TODO wait onMediaFrame end
		for (auto listener : listeners)
			listener->onEnded(this);
		//Clear listeners
		listeners.clear();
		
		stopped = true;
	});
}

void MediaFrameListenerBridge::SetMaxDelayMs(std::chrono::milliseconds maxDelayMs)
{
	UltraDebug("-MediaFrameListenerBridge::SetMaxDelayMs() [delay:%lld]", maxDelayMs.count());

	timeService.Async([=](auto now) {
		maxDispatchingDelayMs = maxDelayMs;
		if (dispatchingDelayMs > maxDispatchingDelayMs)
			dispatchingDelayMs = maxDispatchingDelayMs;
	});
}

void MediaFrameListenerBridge::SetDelayMs(std::chrono::milliseconds delayMs)
{
	UltraDebug("-MediaFrameListenerBridge::SetDelayMs() [delay:%lld]", delayMs.count());

	timeService.Async([=](auto now) { 
		// Set a delay limit so the queue wouldn't grow without control if something is wrong, i.e. the packet
		// time info was not valid
		if (delayMs > maxDispatchingDelayMs)
		{
			Warning("-MediaFrameListenerBridge::SetDelayMs() too large dispatch delay:%lldms max:%lldms", delayMs.count(), maxDispatchingDelayMs.count());
			dispatchingDelayMs = maxDispatchingDelayMs;
		}
		else
		{
			dispatchingDelayMs = delayMs;
		}
	});
}

void MediaFrameListenerBridge::AddListener(RTPIncomingMediaStream::Listener* listener)
{
	Debug("-MediaFrameListenerBridge::AddListener() [this:%p,listener:%p]\n", this, listener);
	timeService.Async([=](auto now){
		listeners.insert(listener);
	});
}

void MediaFrameListenerBridge::RemoveListener(RTPIncomingMediaStream::Listener* listener)
{
	Debug("-MediaFrameListenerBridge::RemoveListener() [this:%p,listener:%p]\n", this, listener);
	timeService.Sync([=](auto now){
		listeners.erase(listener);
	});
}

void MediaFrameListenerBridge::onMediaFrame(DWORD ignored, const MediaFrame& frame)
{
	timeService.Async([=, frame = std::shared_ptr<MediaFrame>(frame.Clone())] (auto now){
		
		//Multiplex
		for (auto& listener : mediaFrameListeners)
			listener->onMediaFrame(*frame);

		//Dispatch any READY packets now
		std::vector<RTPPacket::shared> sending;		
		while (packets.size())
		{
			const auto& packetInfo = packets.front();
			if (packetInfo.scheduled > now) break;
			
			//Add to sending packets
			sending.push_back(packetInfo.packet);
			//remove it from pending packets
			packets.pop();
		}
		//Dispatch RTP packets
		Dispatch(sending);

		//Updated last dispatched 
		lastSent = now;
		
		if (stopped) return;

		//Dispatch packets again
		dispatchTimer->Again(0ms);

		//Check
		if (!frame->HasRtpPacketizationInfo())
			//Error
			return;

		//If we need to reset
		if (reset)
		{
			//Reset first paquet seq num and timestamp
			firstTimestamp = NoTimestamp;
			//Store the last send ones
			baseTimestamp = lastTimestamp;
			//Reseted
			reset = false;
		}

		//UPdate size for video
		if (frame->GetType() == MediaFrame::Video)
		{
			//Convert
			auto  videoFrame = std::static_pointer_cast<VideoFrame>(frame);
			//Set width and height
			width = videoFrame->GetWidth();
			height = videoFrame->GetHeight();

			// Increase bframes
			accumulatorBFrames.Update(now.count(), videoFrame->IsBFrame() ? 1 : 0);
			// Increase iframes
			accumulatorIFrames.Update(now.count(), videoFrame->IsIntra() ? 1 : 0);
			
			bool isPFrame = !(videoFrame->IsIntra() || videoFrame->IsBFrame());
			accumulatorPFrames.Update(now.count(), isPFrame ? 1 : 0);
		}

		//Get info
		const MediaFrame::RtpPacketizationInfo& info = frame->GetRtpPacketizationInfo();

		const BYTE *frameData = NULL;
		DWORD frameSize = 0;
		QWORD rate = 1000;

		//Depending on the type
		type = frame->GetType();
		switch(type)
		{
			case MediaFrame::Audio:
			{
				//get audio frame
				AudioFrame* audio = (AudioFrame*)frame.get();
				// Note: This may truncate UNKNOWN but we do that many places elsewhere treating -1 == 0xFF == UNKNOWN so being consistent here as well
				codec = audio->GetCodec();
				//Get data
				frameData = audio->GetData();
				//Get size
				frameSize = audio->GetLength();
				//Set correct clock rate for audio codec
				rate = AudioCodec::GetClockRate(audio->GetCodec());
				break;
			}
			case MediaFrame::Video:
			{
				//get Video frame
				VideoFrame* video = (VideoFrame*)frame.get();
				// Note: This may truncate UNKNOWN but we do that many places elsewhere treating -1 == 0xFF == UNKNOWN so being consistent here as well
				codec = video->GetCodec();
				//Get data
				frameData = video->GetData();
				//Get size
				frameSize = video->GetLength();
				//Set clock rate
				rate = 90000;
				break;
			}
			default:
				return;

		}
		
		// Calculate the scheduled time
		auto scheduled = now + dispatchingDelayMs;
		if (!packets.empty())
		{
			// Ensure scheduled time increasing
			scheduled = std::max(scheduled, packets.back().scheduled + std::chrono::milliseconds(1));
		}

		//Get frame reception time
		uint64_t time = frame->GetTime();

		//Increase stats
		numFrames++;
		totalBytes += frameSize;

		//Increate accumulators.
		accumulatorFrames.Update(now.count(),1);
		//Update bitrate acumulator
		acumulator.Update(now.count(),frameSize);

		//Get scheduled time in ms
		uint64_t ms = scheduled.count();
		//Waiting time uses scheduled time
		waited.Update(ms, ms>time ? ms-time : 0);
		//Get bitrate in bps
		bitrate = acumulator.GetInstant()*8;

		//Check if it the first received packet
		if (firstTimestamp==NoTimestamp)
		{
			//If we have a time offest from last sent packet
			if (lastTime)
			{
				//Get offset
				QWORD offset = std::max<QWORD>((QWORD)(getTimeDiff(lastTime)*frame->GetClockRate()/1E6),1ul);
				//Calculate time difd and add to the last sent timestamp
				baseTimestamp = lastTimestamp + offset;
			}
			//Get first timestamp
			firstTimestamp = frame->GetTimeStamp();
		}

		//Calculate total length
		uint32_t pendingLength = 0;
		for (size_t i=0;i<info.size();i++)
			//Get total length
			pendingLength += info[i].GetTotalLength();

		//Calculate each packet duration
		uint32_t pendingDuration = frame->GetDuration() * 1000 / rate;
			
		//For each one
		for (size_t i=0;i<info.size();i++)
		{
			
			//Get packet
			const MediaFrame::RtpPacketization& rtp = info[i];

			//Create rtp packet
			auto packet = std::make_shared<RTPPacket>(frame->GetType(),codec);

			//Make sure it is enought length
			if (rtp.GetTotalLength()>packet->GetMaxMediaLength())
				//Error
				continue;
			//Set src
			packet->SetSSRC(ssrc);
			packet->SetExtSeqNum(extSeqNum++);
			//Set data
			packet->SetPayload(frameData+rtp.GetPos(),rtp.GetSize());
			//Add prefix
			packet->PrefixPayload(rtp.GetPrefixData(),rtp.GetPrefixLen());
			//Calculate timestamp
			lastTimestamp = baseTimestamp + (frame->GetTimeStamp()-firstTimestamp);
			//Set other values
			packet->SetClockRate(rate);
			packet->SetExtTimestamp(lastTimestamp*rate/frame->GetClockRate());
			packet->SetAbsoluteCaptureTime(frame->GetSenderTime() ? frame->GetSenderTime() : time);

			//Check
			if (i+1==info.size())
				//last
				packet->SetMark(true);
			else
				//No last
				packet->SetMark(false);
			

			//Increase stats
			numPackets++;
			//Set last sent time
			lastTime = getTime();

			//Fill payload descriptors
			if (frame->GetType()==MediaFrame::Video)
			{
				//get Video frame
				VideoFrame* video = (VideoFrame*)frame.get();

				//TODO: move out of here
				VideoLayerSelector::GetLayerIds(packet);

				//Get media frame target bitrate or hint
				auto targetBitrate = video->GetTargetBitrate() ? video->GetTargetBitrate() : targetBitrateHint;

				//If video has a target bitrate and it is the first packet of an intra frame
				if (i==0 && video->IsIntra() && targetBitrate)
				{
					//Create VLA info
					VideoLayersAllocation videoLayersAllocation = {
						0,
						1,
						{
							{
								0,
								0,
								std::vector<uint16_t>{ (uint16_t)targetBitrate },
								video->GetWidth(),
								video->GetHeight(),
								video->GetTargetFps()
							}
						}
					};
											
					//Set it in packet
					packet->SetVideoLayersAllocation(videoLayersAllocation);
				}
			}

			//If it the media frame has config
			if (frame->HasCodecConfig())
				//Set it in the rtp packet
				packet->config = frame->GetCodecConfig();

			//calculate packet duration based on relative size to keep bitrate smooth
			const uint32_t packetDuration = (smooth && pendingLength > 0) ? pendingDuration * info[i].GetTotalLength() / pendingLength : 0;

			//UltraDebug("-MediaFrameListenerBridge::onMediaFrame() [this:%p,extSeqNum:%d,pending:%d,duration:%dms,total:%d,total:%dms\n", this, extSeqNum-1, pendingLength, packetDuration, info[i].GetTotalLength(), packetDuration);

			//Insert it
			packets.emplace(scheduled, packet, std::chrono::milliseconds(packetDuration));

			//Recalcualte pending
			pendingLength -= info[i].GetTotalLength();
			pendingDuration -= packetDuration;

		}
	});
}

void MediaFrameListenerBridge::Dispatch(const std::vector<RTPPacket::shared>& packets)
{
	if (!muted && packets.size())
		//Dispatch it
		for (auto listener : listeners)
			listener->onRTP(this, packets);
}

void MediaFrameListenerBridge::Reset()
{
	timeService.Async([=](auto now){
		reset = true;
	});
}

void MediaFrameListenerBridge::Update()
{
	Update(getTimeMS());
}


void MediaFrameListenerBridge::UpdateAsync(std::function<void(std::chrono::milliseconds)> callback)
{
	//Update it sync
	timeService.Async([=](auto now) {
		//Update bitrate acumulator
		acumulator.Update(now.count());
		//Get bitrate in bps
		bitrate		= acumulator.GetInstant()*8;
		///Get value
		minWaitedTime	= waited.GetMinValueInWindow();
		//Get value
		maxWaitedTime	= waited.GetMaxValueInWindow();
		//Get value
		avgWaitedTime	= waited.GetInstantMedia();
		//Get packets and frames delta
		numFramesDelta	= accumulatorFrames.GetInstant();
		numPacketsDelta	= accumulatorPackets.GetInstant();
		
		iframes = accumulatorIFrames.GetAcumulated();
		iframesDelta = accumulatorIFrames.GetInstant();
		
		bframes = accumulatorBFrames.GetAcumulated();
		bframesDelta = accumulatorBFrames.GetInstant();
		
		pframes = accumulatorPFrames.GetAcumulated();
		pframesDelta = accumulatorPFrames.GetInstant();
	}, callback);
}

void MediaFrameListenerBridge::Update(QWORD)
{
	timeService.Sync([=](auto now){
		//Update bitrate acumulator
		acumulator.Update(now.count());
		//Get bitrate in bps
		bitrate		= acumulator.GetInstant()*8;
		///Get value
		minWaitedTime	= waited.GetMinValueInWindow();
		//Get value
		maxWaitedTime	= waited.GetMaxValueInWindow();
		//Get value
		avgWaitedTime	= waited.GetInstantMedia();
		//Get packets and frames delta
		numFramesDelta	= accumulatorFrames.GetInstant();
		numPacketsDelta	= accumulatorPackets.GetInstant();
		
		iframes = accumulatorIFrames.GetAcumulated();
		iframesDelta = accumulatorIFrames.GetInstant();

		bframes = accumulatorBFrames.GetAcumulated();
		bframesDelta = accumulatorBFrames.GetInstant();
		
		pframes = accumulatorPFrames.GetAcumulated();
		pframesDelta = accumulatorPFrames.GetInstant();
	});
}

void MediaFrameListenerBridge::AddMediaListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-MediaFrameListenerBridge::AddMediaListener() [this:%p,listener:%p]\n", this, listener.get());

	timeService.Async([=](auto now){
		//Add to set
		mediaFrameListeners.insert(listener);
	});
}

void MediaFrameListenerBridge::RemoveMediaListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-MediaFrameListenerBridge::RemoveMediaListener() [this:%p,listener:%p]\n", this, listener.get());

	timeService.Sync([=](auto now){
		//Remove from set
		mediaFrameListeners.erase(listener);
	});
}


void MediaFrameListenerBridge::Mute(bool muting)
{
	//Log
	Debug("-MediaFrameListenerBridge::Mute() | [muting:%d]\n", muting);

	//Update state
	muted = muting;
}

void MediaFrameListenerBridge::SetTargetBitrateHint(uint32_t targetBitrateHint)
{
	//Log
	Debug("-MediaFrameListenerBridge::SetTargetBitrateHint() | [targetBitrateHint:%d]\n", targetBitrateHint);

	//Update hint
	this->targetBitrateHint = targetBitrateHint;
}