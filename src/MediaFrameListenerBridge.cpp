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

	//If we are doing bitrate smoothing
	if (smooth)
		//Create packet dispatch timer
		dispatchTimer = timeService.CreateTimer([=](auto now){
			//If there are no pending packets
			if (packets.empty())
				//Done
				return;

			//Get first packet to send
			auto& [packet,duration] = packets.front();

			//Dispatch RTP packet
			Dispatch(packet);

			//remove it
			packets.pop();

			//Reschedule
			dispatchTimer->Again(std::chrono::milliseconds(duration));
		});
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
	Debug("-MediaFrameListenerBridge::AddListener() [listener:%p,this:%p]\n",listener,this);
	timeService.Async([=](auto now){
		listeners.insert(listener);
	});
}

void MediaFrameListenerBridge::RemoveListener(RTPIncomingMediaStream::Listener* listener)
{
	Debug("-MediaFrameListenerBridge::RemoveListener() [listener:%p,this:%p]\n",listener,this);
	timeService.Sync([=](auto now){
		listeners.erase(listener);
	});
}

void MediaFrameListenerBridge::onMediaFrame(const MediaFrame& frame)
{
	timeService.Async([=,cloned = frame.Clone()](auto now){
		
		std::unique_ptr<MediaFrame> frame(cloned);
		
		//Multiplex
		for (auto listener : mediaFrameListenerss)
			listener->onMediaFrame(*frame);

		//For all pending packets
		while(!packets.empty())
		{
			//Get first
			auto& [packet, duration] = packets.front();
			//Dipatch it
			Dispatch(packet);
			//remove
			packets.pop();
		}

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

		DWORD frameLength = 0;
		//Calculate total length
		for (size_t i=0;i<info.size();i++)
			//Get total length
			frameLength += info[i].GetTotalLength();

		DWORD current = 0;

		//Calculate each packet duration
		uint32_t frameDuration = frame->GetDuration() * 1000 / rate;

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
			//Calculate partial lenght
			current += rtp.GetPrefixLen()+rtp.GetSize();

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

			//If doing smooting
			if (smooth)
			{
				//calculate packet duration based on relative size to keep bitrate smooth
				const uint32_t packetDuration = std::max<uint32_t>(frameDuration * packet->GetMediaLength() / frameLength,1);

				//Log("-%d %d %dms %d %dms\n", extSeqNum-1, frameLength, frameDuration, packet->GetMediaLength(), packetDuration);

				//Insert it
				packets.emplace(packet,packetDuration);
			} else {
				Dispatch(packet);
			}
		}
	
		//If doing smoothing
		if (smooth)
			//Dispatch first packet now
			dispatchTimer->Again(0ms);
	});
}

void MediaFrameListenerBridge::Dispatch(const RTPPacket::shared& packet)
{
	if (!muted)
		//Dispatch it
		for (auto listener : listeners)
			listener->onRTP(this, packet);
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

void MediaFrameListenerBridge::AddMediaListener(MediaFrame::Listener *listener)
{
	timeService.Async([=](auto now){
		//Add to set
		mediaFrameListenerss.insert(listener);
	});
}

void MediaFrameListenerBridge::RemoveMediaListener(MediaFrame::Listener *listener)
{
	timeService.Sync([=](auto now){
		//Remove from set
		mediaFrameListenerss.erase(listener);
	});
}


void MediaFrameListenerBridge::Mute(bool muting)
{
	//Log
	UltraDebug("-MediaFrameListenerBridge::Mute() | [muting:%d]\n", muting);

	//Update state
	muted = muting;
}