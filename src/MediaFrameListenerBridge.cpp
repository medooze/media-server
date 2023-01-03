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
	waited(1000)
{
	Debug("-MediaFrameListenerBridge::MediaFrameListenerBridge() [this:%p]\n", this);

	//Create packet dispatch timer
	dispatchTimer = timeService.CreateTimer([=](auto now){

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
			//Get first packet to send
			const auto& [packet,duration] = packets.front();
			//Increase accumulated time
			accumulated += duration;
			//Add to sending packets
			sending.push_back(packet);
			//remove it from pending packets
			packets.pop();
		}

		//UltraDebug("-MediaFrameListenerBridge::dispatchTimer() [queue:%d,sending:%d,period:%llu,accumulated:%llu,next:%d,smooth:%d]\n", packets.size(), sending.size(), period.count(), accumulated.count(), packets.size() && accumulated > period ? (accumulated - period).count(): -1, this->smooth);

		//Dispatch RTP packets
		Dispatch(sending);

		//If we have packets in the queue
		if (packets.size() && accumulated > period)
			//Reschedule
			dispatchTimer->Again(std::chrono::milliseconds(accumulated - period));
	});
	dispatchTimer->SetName("MediaFrameListenerBridge::dispatchTimer");
}

MediaFrameListenerBridge::~MediaFrameListenerBridge()
{
	Debug("-MediaFrameListenerBridge::~MediaFrameListenerBridge() [this:%p]\n", this);
}

void MediaFrameListenerBridge::Stop()
{
	Debug("-MediaFrameListenerBridge::Stop() [this:%p]\n", this);

	timeService.Sync([=](auto now) {
		//TODO wait onMediaFrame end
		for (auto listener : listeners)
			listener->onEnded(this);
		//Clear listeners
		listeners.clear();
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

void MediaFrameListenerBridge::onMediaFrame(const MediaFrame& frame)
{
	timeService.Async([=, frame = std::shared_ptr<MediaFrame>(frame.Clone())] (auto now){
		
		//Multiplex
		for (auto& listener : mediaFrameListeners)
			listener->onMediaFrame(*frame);

		//Dispatch any pending packet now
		std::vector<RTPPacket::shared> sending;
		//Remove all packets
		while (packets.size())
		{
			//Get first packet to send
			const auto& [packet, duration] = packets.front();
			//Add to sending packets
			sending.push_back(packet);
			//remove it from pending packets
			packets.pop();
		}
		//Dispatch RTP packets
		Dispatch(sending);

		//Updated last dispatched 
		lastSent = now;

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

		//Get info
		const MediaFrame::RtpPacketizationInfo& info = frame->GetRtpPacketizationInfo();

		DWORD codec = 0;
		const BYTE *frameData = NULL;
		DWORD frameSize = 0;
		QWORD rate = 1000;

		//Depending on the type
		switch(frame->GetType())
		{
			case MediaFrame::Audio:
			{
				//get audio frame
				AudioFrame* audio = (AudioFrame*)frame.get();
				//Get codec
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
				//Get codec
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

		//Get now in ms
		uint64_t ms = now.count();
		//Get frame reception time
		uint64_t time = frame->GetTime();

		//Increase stats
		numFrames++;
		totalBytes += frameSize;

		//Increate accumulators
		accumulatorFrames.Update(ms,1);
		//Update bitrate acumulator
		acumulator.Update(ms,frameSize);
		//Waiting time
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
			packet->SetAbsoluteCaptureTime(time);
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
			//TODO: move out of here
			if (frame->GetType()==MediaFrame::Video)
				VideoLayerSelector::GetLayerIds(packet);

			//If it the media frame has config
			if (frame->HasCodecConfig())
				//Set it in the rtp packet
				packet->config = frame->GetCodecConfig();

			//calculate packet duration based on relative size to keep bitrate smooth
			const uint32_t packetDuration = smooth ? pendingDuration * info[i].GetTotalLength() / pendingLength : 0;

			//UltraDebug("-MediaFrameListenerBridge::onMediaFrame() [this:%p,extSeqNum:%d,pending:%d,duration:%dms,total:%d,total:%dms\n", extSeqNum-1, pendingLength, packetDuration, info[i].GetTotalLength(), packetDuration);

			//Insert it
			packets.emplace(packet,packetDuration);

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


void MediaFrameListenerBridge::Update(QWORD now)
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
	UltraDebug("-MediaFrameListenerBridge::Mute() | [muting:%d]\n", muting);

	//Update state
	muted = muting;
}
