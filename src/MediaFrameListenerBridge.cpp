#include "video.h"
#include "audio.h"
#include "MediaFrameListenerBridge.h"
#include "VideoLayerSelector.h"
#include "EventLoop.h"


MediaFrameListenerBridge::MediaFrameListenerBridge(DWORD ssrc) : 
	ssrc(ssrc),
	acumulator(1000),
	accumulatorFrames(1000),
	accumulatorPackets(1000),
	waited(1000)
{
	loop.Start();
}

MediaFrameListenerBridge::~MediaFrameListenerBridge()
{
	loop.Sync([=](...){
		for (auto listener : listeners)
			listener->onEnded(this);
	});
}
void MediaFrameListenerBridge::AddListener(RTPIncomingMediaStream::Listener* listener)
{
	Debug("-MediaFrameListenerBridge::AddListener() [listener:%p]\n",listener);
	loop.Async([=](...){
		listeners.insert(listener);
	});
}

void MediaFrameListenerBridge::RemoveListener(RTPIncomingMediaStream::Listener* listener)
{
	Debug("-MediaFrameListenerBridge::RemoveListener() [listener:%p]\n",listener);
	loop.Sync([=](...){
		listeners.erase(listener);
	});
}

void MediaFrameListenerBridge::onMediaFrame(const MediaFrame& frame)
{
	loop.Async([=,cloned = frame.Clone()](...){
		
		std::unique_ptr<MediaFrame> frame(cloned);
		
		//Multiplex
		for (auto listener : mediaFrameListenerss)
			listener->onMediaFrame(*frame);

		//Check
		if (!frame->HasRtpPacketizationInfo())
			//Error
			return;

		//If we need to reset
		if (reset)
		{
			//Reset first paquet seq num and timestamp
			firstTimestamp = 0;
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

		//Get now
		auto now = getTimeMS();
		//Get frame reception time
		auto time = frame->GetTime();

		//Increase stats
		numFrames++;
		totalBytes += frameSize;

		//Increate accumulators
		accumulatorFrames.Update(now,1);
		//Update bitrate acumulator
		acumulator.Update(now,frameSize);
		//Waiting time
		waited.Update(now, now>time ? now-time : 0);
		//Get bitrate in bps
		bitrate = acumulator.GetInstant()*8;

		//Check if it the first received packet
		if (!firstTimestamp)
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

			for (auto listener : listeners)
				listener->onRTP(this,packet);
		}
	});
}

void MediaFrameListenerBridge::Reset()
{
	loop.Async([=](...){
		reset = true;
	});
}

void MediaFrameListenerBridge::Update()
{
	Update(getTimeMS());
}


void MediaFrameListenerBridge::Update(QWORD now)
{
	loop.Sync([=](...){
		//Update bitrate acumulator
		acumulator.Update(now);
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
	loop.Async([=](...){
		//Add to set
		mediaFrameListenerss.insert(listener);
	});
}

void MediaFrameListenerBridge::RemoveMediaListener(MediaFrame::Listener *listener)
{
	loop.Sync([=](...){
		//Remove from set
		mediaFrameListenerss.erase(listener);
	});
}
