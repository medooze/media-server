#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "log.h"
#include "mp4recorder.h"
#include "h264/h264.h"
#include "aac/aacconfig.h"
#include "opus/opusconfig.h"



mp4track::mp4track(MP4FileHandle mp4) :
	mp4(mp4)
{
}

void mp4track::SetTrackName(const std::string& name)
{
	MP4SetTrackName(mp4, track, name.c_str());
}

int mp4track::CreateAudioTrack(AudioCodec::Type codec, DWORD rate, bool disableHints)
{
	Log("-mp4track::CreateAudioTrack() [codec:%d]\n",codec);
	
	BYTE type;

	//Check the codec
	switch (codec)
	{
		case AudioCodec::PCMU:
		{
			// Create audio track
			track = MP4AddULawAudioTrack(mp4,rate);
			// Set channel and sample properties
			MP4SetTrackIntegerProperty(mp4, track, "mdia.minf.stbl.stsd.ulaw.channels", 1);
			MP4SetTrackIntegerProperty(mp4, track, "mdia.minf.stbl.stsd.ulaw.sampleSize", 8);
			
			//If hints are not disabled
			if (!disableHints)
			{
				// Create audio hint track
				hint = MP4AddHintTrack(mp4, track);
				// Set payload type for hint track
				type = 0;
				MP4SetHintTrackRtpPayload(mp4, hint, "PCMU", &type, 0, NULL, 1, 0);
			}
			
			break;
		}
		case AudioCodec::PCMA:
		{
			// Create audio track
			track = MP4AddALawAudioTrack(mp4,rate);
			// Set channel and sample properties
			MP4SetTrackIntegerProperty(mp4, track, "mdia.minf.stbl.stsd.alaw.channels", 1);
			MP4SetTrackIntegerProperty(mp4, track, "mdia.minf.stbl.stsd.alaw.sampleSize", 8);
			
			//If hints are not disabled
			if (!disableHints)
			{
				// Create audio hint track
				hint = MP4AddHintTrack(mp4, track);
				// Set payload type for hint track
				type = 8;
				MP4SetHintTrackRtpPayload(mp4, hint, "PCMA", &type, 0, NULL, 1, 0);
			}
			break;
		}
		case AudioCodec::OPUS:
		{
#ifdef MP4_OPUS_AUDIO_TYPE
			// Create audio track
			track = MP4AddOpusAudioTrack(mp4, rate, 2, 640);
#else      
			// Create audio track
			track = MP4AddAudioTrack(mp4, rate, 1024, MP4_PRIVATE_AUDIO_TYPE);
#endif       
			//If hints are not disabled
			if (!disableHints)
			{
				// Create audio hint track
				hint = MP4AddHintTrack(mp4, track);
				// Set payload type for hint track
				type = 102;
				MP4SetHintTrackRtpPayload(mp4, hint, "OPUS", &type, 0, NULL, 1, 0);
			}
			break;
		}
		case AudioCodec::AAC:
		{
			BYTE config[24];
			// Create audio track
			track = MP4AddAudioTrack(mp4, rate, 1024, MP4_MPEG2_AAC_LC_AUDIO_TYPE);
			//Create AAC config
			AACSpecificConfig aacSpecificConfig(rate,1);
			//Serialize it
			auto size = aacSpecificConfig.Serialize(config,sizeof(config));
			// Set channel and sample properties
			MP4SetTrackESConfiguration(mp4, track,config,size);
			// No hint track
			hint = 0;
			break;
		}
		default:
			return 0;
	}
	
	//Sotore clock rate
	clockrate = rate;

	return track;
}

int mp4track::CreateVideoTrack(VideoCodec::Type codec, DWORD rate, int width, int height, bool disableHints)
{
	
	Log("-mp4track::CreateVideoTrack() [codec:%d,rate:%d,width:%d,height:%d]\n",codec,rate,width,height);
	
	BYTE type;

	//Check the codec
	switch (codec)
	{
		case VideoCodec::H264:
		{
			// Should parse video packet to get this values
			unsigned char AVCProfileIndication 	= 0x42;	//Baseline
			unsigned char AVCLevelIndication	= 0x0D;	//1.3
			unsigned char AVCProfileCompat		= 0xC0;
			MP4Duration h264FrameDuration		= 1.0/30;
			// Create video track
			track = MP4AddH264VideoTrack(mp4, rate, h264FrameDuration, width, height, AVCProfileIndication, AVCProfileCompat, AVCLevelIndication,  3);
			//If hints are not disabled
			if (!disableHints)
			{
				// Create video hint track
				hint = MP4AddHintTrack(mp4, track);
				// Set payload type for hint track
				type = 99;
				MP4SetHintTrackRtpPayload(mp4, hint, "H264", &type, 0, NULL, 1, 0);
			}
			break;
		}
		case VideoCodec::VP8:
		{
			// Should parse video packet to get this values
			MP4Duration hvp8FrameDuration	= 1.0/30;
#ifdef MP4_VP8_VIDEO_TYPE      
			// Create video track
			track = MP4AddVP8VideoTrack(mp4, rate, hvp8FrameDuration, width, height);
#else
			// Create video track
			track = MP4AddVideoTrack(mp4, rate, hvp8FrameDuration, width, height, MP4_PRIVATE_VIDEO_TYPE);
#endif
			//If hints are not disabled
			if (!disableHints)
			{
				// Create video hint track
				hint = MP4AddHintTrack(mp4, track);
				// Set payload type for hint track
				type = 101;
				MP4SetHintTrackRtpPayload(mp4, hint, "VP8", &type, 0, NULL, 1, 0);
			}
			break;
		}
		case VideoCodec::VP9:
		{
			// Should parse video packet to get this values
			MP4Duration vp9FrameDuration	= 1.0/30;
#ifdef MP4_VP9_VIDEO_TYPE      
			// Create video track
			track = MP4AddVP9VideoTrack(mp4, rate, vp9FrameDuration, width, height);
#else
			// Create video track
			track = MP4AddVideoTrack(mp4, rate, vp9FrameDuration, width, height, MP4_PRIVATE_VIDEO_TYPE);
#endif
			//If hints are not disabled
			if (!disableHints)
			{
				// Create video hint track
				hint = MP4AddHintTrack(mp4, track);
				// Set payload type for hint track
				type = 102;
				MP4SetHintTrackRtpPayload(mp4, hint, "VP9", &type, 0, NULL, 1, 0);
				//If hints are not disabled
			}
			break;
		}		
		default:
			return Error("-Codec %s not supported yet\n",VideoCodec::GetNameFor(codec));
	}
	
	//Check if it has dimensions
	hasDimensions = width && height;
	
	//Sotore clock rate
	clockrate = rate;
	
	//OK
	return 1;
}

int mp4track::CreateTextTrack()
{
	//Create subtitle track
	track = MP4AddSubtitleTrack(mp4,1000,0,0);
	
	//Sotore clock rate
	clockrate = 1000;
	
	//OK
	return 1;
}

int mp4track::FlushAudioFrame(AudioFrame* frame,DWORD duration)
{
	//Log("-mp4track::FlushAudioFrame() [duration:%u,length:%d]\n",duration,frame->GetLength());
	// Save audio frame
	MP4WriteSample(mp4, track, frame->GetData(), frame->GetLength(), duration, 0, 1);

	//If as rtp info
	if (hint)
	{
		// Add rtp hint
		MP4AddRtpHint(mp4, hint);

		///Create packet
		MP4AddRtpPacket(mp4, hint, 0, 0);

		// Set full frame as data
		MP4AddRtpSampleData(mp4, hint, sampleId, 0, frame->GetLength());

		// Write rtp hint
		MP4WriteRtpHint(mp4, hint, duration, 1);
	}

	// Delete old one
	delete frame;
	//Stored
	return 1;
}

int mp4track::WriteAudioFrame(AudioFrame &audioFrame)
{
	//Store old one
	AudioFrame* prev = (AudioFrame*)frame;

	//Clone new one and store
	frame = audioFrame.Clone();

	//Check if we had and old frame
	if (!prev)
		//Exit
		return 0;
	
	//One more frame
	sampleId++;

	//Get frame duration
	DWORD duration = prev->GetDuration();
	
	//Get frame timestamp delta
	DWORD delta = frame->GetTimeStamp()-prev->GetTimeStamp();
	
	//If not set
	if (!duration)
		//Get timestamp delta
		duration = delta;
	
	//If not sync source yet
	if (!firstSenderTime)
	{
		//Try get it
		firstSenderTime  = prev->GetSenderTime();
		firstTimestamp   = prev->GetTimeStamp();
	} 
	
	//Update last
	if (prev->GetSenderTime())
	{
		//Only valid times
		lastSenderTime  = prev->GetSenderTime();
		lastTimestamp   = prev->GetTimeStamp();
	}
	
	//If it is Opus and there are missing packets
	if (prev->GetCodec()==AudioCodec::OPUS && duration<delta && prev->GetLength()>0)
	{
		//Get opus toc
		auto [mode, bandwidth, frameSize, stereo, codeNumber ] = OpusTOC::TOC(prev->GetData()[0]);
		
		//Flush sample
		FlushAudioFrame((AudioFrame *)prev,duration);
		
		//Create missing audio frame
		auto missing = std::make_shared<Buffer>(3);
		
		//Set toc
		missing->GetData()[0] = OpusTOC::GetTOC(mode,bandwidth,frameSize,stereo,OpusTOC::Arbitrary);
		
		//Missing
		missing->GetData()[1] = 0x81; //vbr 1 packet
		missing->GetData()[2] = 0x00; //0 length -> missing or PLC
		
		//Set size
		missing->SetSize(3);
		
		//Fill the gap
		while(duration+frameSize<=delta)
		{
			//Flush missing
			FlushAudioFrame(new AudioFrame(AudioCodec::OPUS, missing),frameSize);
			//Increase timestamp
			duration += frameSize;
		}
	} else {
		//Flush sample
		FlushAudioFrame((AudioFrame *)prev,duration);
	}
	
	//Exit
	return 1;
}

int mp4track::FlushVideoFrame(VideoFrame* frame,DWORD duration)
{
	//Log("-mp4track::FlushVideoFrame() [duration:%u,width:%d,height:%d%s]\n",duration, frame->GetWidth(), frame->GetHeight(), frame->IsIntra() ? ",intra" : "");
	// Save video frame
	MP4WriteSample(mp4, track, frame->GetData(), frame->GetLength(), duration, 0, frame->IsIntra());

	//Check if we have rtp data
	if (hint && frame->HasRtpPacketizationInfo())
	{
		//Get list
		const MediaFrame::RtpPacketizationInfo& rtpInfo = frame->GetRtpPacketizationInfo();
		//Add hint for frame
		MP4AddRtpHint(mp4, hint);
		//Get iterator
		MediaFrame::RtpPacketizationInfo::const_iterator it = rtpInfo.begin();
		//Latest?
		bool last = (it==rtpInfo.end());

		//Iterate
		while(!last)
		{
			//Get rtp packet
			const MediaFrame::RtpPacketization& rtp = *(it++);
			//is last?
			last = (it==rtpInfo.end());

			//Create rtp packet
			MP4AddRtpPacket(mp4, hint, last, 0);

			//Prefix data can't be longer than 14bytes per mp4 spec
			if (rtp.GetPrefixLen() && rtp.GetPrefixLen()<14)
				//Add rtp data
				MP4AddRtpImmediateData(mp4, hint, rtp.GetPrefixData(), rtp.GetPrefixLen());

			//Add rtp data
			MP4AddRtpSampleData(mp4, hint, sampleId, rtp.GetPos(), rtp.GetSize());

			//It is h264 and we still do not have SPS or PPS?
			// only check full full naltypes
			if (frame->GetCodec()==VideoCodec::H264 && (!hasSPS || !hasPPS) && !rtp.GetPrefixLen() && rtp.GetSize()>1)
			{
				//Get rtp data pointer
				const BYTE *data = frame->GetData()+rtp.GetPos();
				//Check nal type
				BYTE nalType = data[0] & 0x1F;
				//Get nal data
				const BYTE *nalData = data+1;
				DWORD nalSize = rtp.GetSize()-1;
				
				//If it a SPS NAL
				if (nalType==0x07)
					//Add it
					AddH264SequenceParameterSet(nalData,nalSize);
				//If it is a PPS NAL
				else if (nalType==0x08)
					//Add it
					AddH264PictureParameterSet(nalData,nalSize);
			}
		}
		//Save rtp
		MP4WriteRtpHint(mp4, hint, duration, frame->IsIntra());
	}

	// Delete old one
	delete frame;
	//Stored
	return 1;
}

void mp4track::AddH264SequenceParameterSet(const BYTE* data, DWORD size)
{
	//If it a SPS NAL
	if (hasSPS)
		//Do noting
		return;	
	
	Debug("-mp4track::AddH264SequenceParameterSet() | Got SPS\n");
	//Add it
	MP4AddH264SequenceParameterSet(mp4,track,data,size);
	
	//No need to search more
	hasSPS = true;

	//IF we dont have widht or height
	if (!hasDimensions)
	{
		H264SeqParameterSet sps;
		//DEcode SPS
		if (sps.Decode(data,size))
		{
			Debug("-mp4track::AddH264SequenceParameterSet() | Got size [%dx%d]\n", sps.GetWidth(), sps.GetHeight());
			
			//Update width an height
			MP4SetTrackIntegerProperty(mp4,track,"mdia.minf.stbl.stsd.avc1.width", sps.GetWidth());
			MP4SetTrackIntegerProperty(mp4,track,"mdia.minf.stbl.stsd.avc1.height", sps.GetHeight());
			//Check if it has dimensions
			hasDimensions = sps.GetWidth() && sps.GetHeight();
		}
	}
}

void mp4track::AddH264PictureParameterSet(const BYTE* data, DWORD size)
{
	//If it a PPS NAL
	if (hasPPS)
		//Do noting
		return;	
	
	Debug("-mp4track::AddH264PictureParameterSet() | Got PPS\n");
	//Add it
	MP4AddH264PictureParameterSet(mp4,track,data,size);
	//No need to search more
	hasPPS = true;
}

int mp4track::WriteVideoFrame(VideoFrame& videoFrame)
{
	//Store old one
	VideoFrame* prev = (VideoFrame*)frame;

	//Clone new one and store
	frame = videoFrame.Clone();

	//Check if we had and old frame
	if (!prev)
		//Exit
		return 0;

	//One mor frame
	sampleId++;

	//Get frame duration
	DWORD duration = prev->GetDuration();
	
	//If not set
	if (!duration)
		//calculate it
		duration = frame->GetTimeStamp()-prev->GetTimeStamp();

	//Flush frame
	FlushVideoFrame(prev,duration);
	
	//Not writed
	return 1;
}

int mp4track::FlushTextFrame(TextFrame *frame, DWORD duration)
{
	//Set the duration of the frame on the screen
	MP4Duration frameduration = duration;

	//If it is too much
	if (frameduration>7000)
		//Cut it
		frameduration = 7000;

	//Get text utf8 size
	DWORD size = frame->GetLength();

	//Create data to send
	std::vector<BYTE> data(size+2);

	//Set size
	data[0] = size>>8;
	data[1] = size & 0xFF;
	//Copy text
	memcpy(data.data()+2,frame->GetData(),frame->GetLength());
	//Log
	Debug("-mp4track::FlushTextFrame() [timestamp:%lu,duration:%lu,size:%u]\n]",frame->GetTimeStamp(),frameduration,size+2);
	//Write sample
	MP4WriteSample( mp4, track, data.data(), size+2, frameduration, 0, false );

	//If we have to clear the screen after 7 seconds
	if (duration-frameduration>0)
	{
		//Log
		Debug("-mp4track::FlushTextFrame() empty text [timestamp:%lu,duration:%lu]\n]",frame->GetTimeStamp()+frameduration,duration-frameduration);
		//Put empty text
		data[0] = 0;
		data[1] = 0;

		//Write sample
		MP4WriteSample( mp4, track, data.data(), 2, duration-frameduration, 0, false );
	}
	// Delete old one
	delete frame;
	//Stored
	return 1;
}

int mp4track::WriteTextFrame(TextFrame& textFrame)
{
	//Store old one
	TextFrame* prev = (TextFrame*)frame;

	//Clone new one and store
	frame = textFrame.Clone();

	//Check if we had and old frame
	if (!prev)
		//Exit
		return 0;

	//One mor frame
	sampleId++;

	//Get samples
	DWORD duration = frame->GetTimeStamp()-prev->GetTimeStamp();

	//Flush frame
	FlushTextFrame((TextFrame*)prev,duration);

	//writed
	return 1;
}

int mp4track::Close()
{
	Debug("-mp4track::Close()\n");
	//If we got frame
	if (frame)
	{
		//If we have pre frame
		switch (frame->GetType())
		{
			case MediaFrame::Audio:
				//Flush it
				FlushAudioFrame((AudioFrame*)frame,frame->GetClockRate());
				break;
			case MediaFrame::Video:
				//Flush it
				FlushVideoFrame((VideoFrame*)frame,frame->GetDuration());
				break;
			case MediaFrame::Text:
				//Flush it
				FlushTextFrame((TextFrame*)frame,frame->GetClockRate());
				break;
			case MediaFrame::Unknown:
				//Nothing
				break;
		}
		//NO frame
		frame = NULL;
	}

	//If we have timing information
	if (firstSenderTime && lastSenderTime && lastSenderTime>firstSenderTime && lastTimestamp>firstTimestamp)
	{
		
		//Get diff in sender time
		QWORD deltaTime = lastSenderTime-firstSenderTime;
		QWORD deltaTimestamp = (lastTimestamp-firstTimestamp)*1000/clockrate;
		//calculate drift rate
		int64_t skew = deltaTime - deltaTimestamp;
		double drift = (double)deltaTimestamp/deltaTime;
		
		//If we have a big drift
		if (std::abs(skew)>40)
		{
			
			//Calculate new clockrate
			uint64_t adjusted = lround(drift*clockrate);
			//Log
			Log("-mp4track::Close() | Clockdrift detecteced, adjusting clockrate [skew:%lldms,dirft:%f,clockrate:%u,adjusted:%lu\n",skew,drift,clockrate,adjusted);
			//Update timescale
			MP4SetTrackIntegerProperty(mp4, track, "mdia.mdhd.timeScale", adjusted);
		}
	}

	return 1;
}

MP4Recorder::MP4Recorder(Listener* listener) :
	listener(listener)
{
	//Create loop
	loop.Start();
}

MP4Recorder::~MP4Recorder()
{
	//If not closed
        if (mp4!=MP4_INVALID_FILE_HANDLE)
		//Close sync
		Close(false);
}

bool MP4Recorder::Create(const char* filename)
{
	Log("-MP4Recorder::Create() Opening mp4 recording [%s]\n",filename);

	//If we are recording
	if (mp4!=MP4_INVALID_FILE_HANDLE)
		//Close
		Close();

	// We have to wait for first I-Frame
	waitVideo = 0;

	// Create mp4 file
	mp4 = MP4Create(filename,0);

	// If failed
	if (mp4 == MP4_INVALID_FILE_HANDLE)
                //Error
		return Error("-Error opening mp4 file for recording\n");

	//Success
	return true;
}

bool MP4Recorder::Record()
{
	return Record(true, false);
}

bool MP4Recorder::Record(bool waitVideo)
{
	return Record(waitVideo, false);
}

bool MP4Recorder::Record(bool waitVideo, bool disableHints)
{
	Log("-MP4Recorder::Record() [waitVideo:%d,disableHints:%d]\n",waitVideo,disableHints);
	
        //Check mp4 file is opened
        if (mp4 == MP4_INVALID_FILE_HANDLE)
                //Error
                return Error("No MP4 file opened for recording\n");
        
	//Do We have to wait for first I-Frame?
	this->waitVideo = waitVideo;
	//Disable hints tracks
	this->disableHints = disableHints;
	
	//Run in thread
	loop.Async([=](auto now){
		//Recording
		recording = true;

		//For all time shift frames
		for (const auto& [ssrc,frame] : timeShiftBuffer)
			//Process it
			processMediaFrame(ssrc,*frame,frame->GetTime());

		//clear buffer
		timeShiftBuffer.clear();
	});
	
	//Exit
	return true;
}

bool MP4Recorder::Stop()
{
	Log("-MP4Recorder::Stop()\n");
	
	//Signal async	
	loop.Async([=](auto now){
		//not recording anymore
		recording = false;
	});
	
	return true;
}

bool MP4Recorder::Close()
{
	//Default is async
	return Close(true);
}

bool MP4Recorder::Close(bool async)
{
	Log("-MP4Recorder::Close()\n");
	
        //Stop always
        auto res = loop.Future([=](auto now){
		Debug(">MP4Recorder::Close() | Async\n");
		
		//Not recording anymore
		recording = false;
		
		//Clear time buffer
		timeShiftBuffer.clear();
		
		//Check mp4 file is opened
		//For each audio track
		for (Tracks::iterator it = audioTracks.begin(); it!=audioTracks.end(); ++it)
			//Close it
			it->second->Close();
		//For each video track
		for (Tracks::iterator it = videoTracks.begin(); it!=videoTracks.end(); ++it)
			//Close it
			it->second->Close();
		//For each text track
		for (Tracks::iterator it = textTracks.begin(); it!=textTracks.end(); ++it)
			//Close it
			it->second->Close();
		
		//If started
		if (mp4!=MP4_INVALID_FILE_HANDLE)
			// Close file
			MP4Close(mp4);

		//Empty file
		mp4 = MP4_INVALID_FILE_HANDLE;
		
		//Triger listener
		if (this->listener)
			//Send event
			this->listener->onClosed();
		
		Debug("<MP4Recorder::Close() | Async\n");
	});
	
	//If sync
	if (!async)
		//Wait
		res.wait();
	
	//NOthing more
	return true;
}

void MP4Recorder::onMediaFrame(const MediaFrame &frame)
{
	onMediaFrame(0,frame);
}

void MP4Recorder::onMediaFrame(DWORD ssrc, const MediaFrame &frame)
{
	
	//run async	
	loop.Async([=,cloned = frame.Clone()](auto now){
		//Check we are recording
		if (recording) 
		{
			//Set now as timestamp
			processMediaFrame(ssrc,*cloned,cloned->GetTime());
			//Delete
			delete cloned;
		} 
		//Check if doing time shift recording
		else if (timeShiftDuration) 
		{
			 //Push it to the end
			timeShiftBuffer.emplace_back(ssrc,cloned);
			//Get time shitft start
			QWORD ini = getTimeMS() - timeShiftDuration;
			//Discard all the timed out frames
			while (!timeShiftBuffer.empty() && timeShiftBuffer.front().second->GetTime()<ini)
				//Delete
				timeShiftBuffer.pop_front();
		} else {
			//Delete
			delete cloned;
		}
	});
}

void MP4Recorder::processMediaFrame(DWORD ssrc, const MediaFrame &frame, QWORD time)
{
	// Check if we have to wait for video
	if (waitVideo && (frame.GetType()!=MediaFrame::Video))
		//Do nothing yet
		return;
	
	//Depending on the codec type
	switch (frame.GetType())
	{
		case MediaFrame::Audio:
		{
			//Convert to audio frame
			AudioFrame &audioFrame = (AudioFrame&) frame;
			//Check if it is the first
			if (first==(QWORD)-1)
			{
				//Log
				Log("-MP4Recorder::processMediaFrame() | Got first frame in audio [time:%llu]\n", time);
				//Set this one as first
				first = time;
				//Triger listener
				if (this->listener)
					//Send event
					this->listener->onFirstFrame(first);
			}

			// Check if we have the audio track
			if (audioTracks.find(ssrc)==audioTracks.end())
			{
				// Calculate time diff since first
				QWORD delta = time > first ? time-first : 0;
				//Create object
				auto audioTrack = std::make_unique<mp4track>(mp4);
				//Create track
				audioTrack->CreateAudioTrack(audioFrame.GetCodec(),audioFrame.GetClockRate(),disableHints);
				//Set name as ssrc
				audioTrack->SetTrackName(std::to_string(ssrc));
				//If it is not first
				if (delta)
				{
					//Calculate duration
					uint64_t duration = delta*audioFrame.GetClockRate()/1000;
					//Create empty text frame
					AudioFrame empty(audioFrame.GetCodec());
					//Set time
					empty.SetTime(time);
					//Set timestamp
					empty.SetTimestamp(audioFrame.GetTimeStamp()>duration ? audioFrame.GetTimeStamp() - duration : 0);
					//Set clock rate
					empty.SetClockRate(audioFrame.GetClockRate());
					//Set duration
					empty.SetDuration(duration);
					//Send first empty packet
					audioTrack->WriteAudioFrame(empty);
				}
				//Add it to map
				audioTracks[ssrc] = std::move(audioTrack);
			}
			// Save audio rtp packet
			audioTracks[ssrc]->WriteAudioFrame(audioFrame);
			break;
		}
		case MediaFrame::Video:
		{
	
			//Convert to video frame
			VideoFrame &videoFrame = (VideoFrame&) frame;

			//If it is intra
			if (waitVideo  && videoFrame.IsIntra())
				//Don't wait more
				waitVideo = 0;

			//Check if it is the first
			if (first==(QWORD)-1)
			{
				//Log
				Log("-MP4Recorder::processMediaFrame() | Got first frame in video [time:%llu]\n", time);
				//Set this one as first
				first = time;
				//If we have listener
				if (this->listener)
					//Send event
					this->listener->onFirstFrame(first);
			}

			//Check if we have to write or not
			if (!waitVideo)
			{
				// Check if we have the audio track
				if (videoTracks.find(ssrc) == videoTracks.end())
				{
					// Calculate time diff since first
					QWORD delta = time > first ? time-first : 0;
					//Create object
					auto videoTrack = std::make_unique<mp4track>(mp4);
					//Create track
					videoTrack->CreateVideoTrack(videoFrame.GetCodec(),videoFrame.GetClockRate(),videoFrame.GetWidth(),videoFrame.GetHeight(),disableHints);
					//Set name as ssrc
					videoTrack->SetTrackName(std::to_string(ssrc));
					
					//If it is h264
					if (videoFrame.GetCodec() == VideoCodec::H264)
					{
						//IF we have pps
						if (h264PPS)
						{
							//Set it
							videoTrack->AddH264SequenceParameterSet(h264PPS->GetData(),h264PPS->GetSize());
							//Create NAL
							BYTE nal[5+h264PPS->GetSize()];
							//Set nal header
							set4(nal,0,h264PPS->GetSize()+1);
							set1(nal,4,0x08);
							//Copy
							memcpy(nal+5,h264PPS->GetData(),h264PPS->GetSize());
							//Add nal
							videoFrame.PrependMedia(nal,sizeof(nal));
							//Add rtp packet
							videoFrame.AddRtpPacket(4,h264PPS->GetSize()+1);
						}
						//IF we have sps
						if (h264SPS)
						{
							//Set it
							videoTrack->AddH264SequenceParameterSet(h264SPS->GetData(),h264SPS->GetSize());
							//Create NAL
							BYTE nal[5+h264SPS->GetSize()];
							//Set nal header
							set4(nal,0,h264SPS->GetSize()+1);
							set1(nal,4,0x07);
							//Copy
							memcpy(nal+5,h264SPS->GetData(),h264SPS->GetSize());
							//Add nal
							videoFrame.PrependMedia(nal,sizeof(nal));
							//Add rtp packet
							videoFrame.AddRtpPacket(4,h264SPS->GetSize()+1);
							
						}
						
					}
					
					//If not the first one
					if (delta)
					{
						//Create empty video frame
						VideoFrame empty(videoFrame.GetCodec(),0);
						//Set time
						empty.SetTime(time);
						//Set timestamp
						empty.SetTimestamp(time*videoFrame.GetClockRate()/1000);
						//Set duration
						empty.SetDuration(delta*videoFrame.GetClockRate()/1000);
						//Size
						empty.SetWidth(videoFrame.GetWidth());
						empty.SetHeight(videoFrame.GetHeight());
						//Set clock rate
						empty.SetClockRate(videoFrame.GetClockRate());
						//first frame must be syncable
						empty.SetIntra(true);
						//Set config
						if (videoFrame.HasCodecConfig()) empty.SetCodecConfig(videoFrame.GetCodecConfigData(),videoFrame.GetCodecConfigSize());
						//Send first empty packet
						videoTrack->WriteVideoFrame(empty);
					}
					
					//Add it to map
					videoTracks[ssrc] = std::move(videoTrack);
				}

				// Save audio rtp packet
				videoTracks[ssrc]->WriteVideoFrame(videoFrame);
			}
			break;
		}
		case MediaFrame::Text:
		{
			// Check if we have the audio track
			if (textTracks.find(ssrc) == textTracks.end())
			{
				//Create object
				auto textTrack = std::make_unique<mp4track>(mp4);
				//Create track
				textTrack->CreateTextTrack();
				//Set name as ssrc
				textTrack->SetTrackName(std::to_string(ssrc));
				//Create empty text frame
				TextFrame empty(0,(BYTE*)NULL,0);
				//Send first empty packet
				textTrack->WriteTextFrame(empty);
				
				textTracks[ssrc] = std::move(textTrack);
			}

			//Check if it is the first
			if (first==(QWORD)-1)
			{
				//Log
				Log("-MP4Recorder::processMediaFrame() | Got first frame in text [time:%llu]\n", time);
				//Set this one as first
				first = time;
				//If we have listener
				if (this->listener)
					//Send event
					this->listener->onFirstFrame(first);
			}
				
			//Convert to audio frame
			TextFrame &textFrame = (TextFrame&) frame;

			// Calculate new timestamp
			QWORD timestamp = time-first;
			//Update timestamp
			textFrame.SetTimestamp(timestamp);

			// Save audio rtp packet
			textTracks[ssrc]->WriteTextFrame(textFrame);
			break;
		}
		case MediaFrame::Unknown:
			//Nothing
			break;
	}
}


bool MP4Recorder::SetH264ParameterSets(const std::string& sprop)
{
	BYTE nal[MTU];
	
	//Log
	Debug("-MP4Recorder::SetH264ParameterSets() [sprop:%s]\n",sprop.c_str());
	
	//Split by ","
	auto start = 0;
	
	//Get next separator
	auto end = sprop.find(',');
	
	//Parse
	while(end!=std::string::npos)
	{
		//Get prop
		auto prop = sprop.substr(start,end-start);
		
		Debug("-MP4Recorder::SetH264ParameterSets() [sprop:%s]\n",prop.c_str());
		
		//Parse and keep space for size
		auto len = av_base64_decode(nal,prop.c_str(),MTU);
		//Check result
		if (len<=0)
			return Error("-MP4Recorder::SetH264ParameterSets() could not decode base64 data [%s]\n",prop.c_str());
		//Get nal type
		BYTE nalType = nal[0] & 0x1F;
		//If it a SPS NAL
		if (nalType==0x07)
			//Add it
			h264SPS.emplace(nal+1,len-1);
		//If it is a PPS NAL
		else if (nalType==0x08)
			//Add it
			h264PPS.emplace(nal+1,len-1);
		
		//Next
		start = end+1;
		//Get nest
		end = sprop.find(',',start);
	}
	//last one
	auto prop = sprop.substr(start,end-start);
	
	Debug("-MP4Recorder::SetH264ParameterSets() [sprop:%s]\n",prop.c_str());
	
	//Parse and keep space for size
	auto len = av_base64_decode(nal,prop.c_str(),MTU);
	//Check result
	if (len<=0)
		return Error("-MP4Recorder::SetH264ParameterSets() could not decode base64 data [%s]\n",prop.c_str());
	::Dump(nal,len);
	//Get nal type
	BYTE nalType = nal[0] & 0x1F;
	//If it a SPS NAL
	if (nalType==0x07)
		//Add it
		h264SPS.emplace(nal+1,len-1);
	//If it is a PPS NAL
	else if (nalType==0x08)
		//Add it
		h264PPS.emplace(nal+1,len-1);
	
	//Done
	return true;
}
