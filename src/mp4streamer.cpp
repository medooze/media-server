#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "log.h"
#include "tools.h"
#include "codecs.h"
#include "rtp.h"
#include "mp4streamer.h"
#include "video.h"
#include "audio.h"



MP4Streamer::MP4Streamer(Listener *listener)
{
	//Save listener
	this->listener = listener;
}

MP4Streamer::~MP4Streamer()
{
	//Check if still opened
	if (opened)
		//Close us
		Close();
}

int MP4Streamer::Open(const char *filename)
{
	//LOg
	Log(">MP4Streamer::Open() [%s]\n",filename);

	//If already opened
	if (opened)
		//Return error
		return Error("-MP4Streamer::Open() | Already opened\n");
	
	// Open mp4 file
	mp4 = MP4Read(filename);

	// If not valid
	if (mp4 == MP4_INVALID_FILE_HANDLE)
		//Return error
		return Error("-MP4Streamer::Open() | Invalid file handle for %s\n",filename);
	
	//No tracks
	audio = NULL;
	video = NULL;
	text = NULL;

	//Iterate thougth tracks
	DWORD i = 0;

	// Get the first hint track
	MP4TrackId hintId = MP4_INVALID_TRACK_ID;

	// Iterate hint tracks
	do
	{
		// Get the next hint track
		hintId = MP4FindTrackId(mp4, i++, MP4_HINT_TRACK_TYPE, 0);

		Log("-MP4Streamer::Open() | Found hint track [hintId:%d]\n", hintId);

		// Get asociated track
		MP4TrackId trackId = MP4GetHintTrackReferenceTrackId(mp4, hintId);

		// Check it's good
		if (trackId != MP4_INVALID_TRACK_ID)
		{
			// Get track type
			const char *type = MP4GetTrackType(mp4, trackId);

			// Get rtp track
			char *name;
			BYTE payload;
			MP4GetHintTrackRtpPayload(mp4, hintId, &name, &payload, NULL, NULL);

			Log("-MP4Streamer::Open() | Streaming media [trackId:%d,type:\"%s\",name:\"%s\",payload:%d]\n", trackId, type, name, payload);

			// Check track type
			if ((strcmp(type, MP4_AUDIO_TRACK_TYPE) == 0) && !audio)
			{
				if (!name)
				{
					Log("-MP4Streamer::Open() | audio stream with empty codec name, skip this stream\n");
					continue;
				}

				// Depending on the name
				if (strcmp("PCMU", name) == 0)
					//Create new audio track
					audio = std::make_unique<MP4RtpTrack>(MediaFrame::Audio,AudioCodec::PCMU,payload,8000);
				else if (strcmp("PCMA", name) == 0)
					//Create new audio track
					audio = std::make_unique<MP4RtpTrack>(MediaFrame::Audio,AudioCodec::PCMA,payload,8000);
				else if (strcmp("OPUS", name) == 0)
					//Create new audio track
					audio = std::make_unique<MP4RtpTrack>(MediaFrame::Audio,AudioCodec::OPUS,payload,48000);
				else
					//Skip
					continue;

				// Get time scale
				audio->timeScale = MP4GetTrackTimeScale(mp4, hintId);

				//Store the other values
				audio->mp4 = mp4;
				audio->hint = hintId;
				audio->track = trackId;
				audio->sampleId = 1;
				audio->packetIndex = 0;

			} else if ((strcmp(type, MP4_VIDEO_TRACK_TYPE) == 0) && !video) {
				if (!name)
				{
					Log("-MP4Streamer::Open() | video stream with empty codec name, skip this stream\n");
					continue;
				}

				if (strcmp("H264", name) == 0)
					//Create new video track
					video = std::make_unique<MP4RtpTrack>(MediaFrame::Video,VideoCodec::H264,payload,90000);
				else if (strcmp("VP8", name) == 0)
					//Create new video track
					video = std::make_unique<MP4RtpTrack>(MediaFrame::Video,VideoCodec::VP8,payload,90000);
				else if (strcmp("VP9", name) == 0)
					//Create new video track
					video = std::make_unique<MP4RtpTrack>(MediaFrame::Video,VideoCodec::VP9,payload,90000);
				else
					continue;
					
				// Get time scale
				video->timeScale = MP4GetTrackTimeScale(mp4, hintId);
				// it's video
				video->mp4 = mp4;
				video->hint = hintId;
				video->track = trackId;
				video->sampleId = 1;
				video->packetIndex = 0;
			}
		} 
	} while (hintId != MP4_INVALID_TRACK_ID);

	// Get the first text
	MP4TrackId textId = MP4FindTrackId(mp4, 0, MP4_TEXT_TRACK_TYPE, 0);

	// Iterate hint tracks
	if (textId != MP4_INVALID_TRACK_ID)
	{
		Log("-MP4Streamer::Open() | Found text track [%d]\n",textId);
		//We have it
		text = std::make_unique<MP4TextTrack>();
		//Set values
		text->mp4 = mp4;
		text->track = textId;
		text->sampleId = 1;
		// Get time scale
		text->timeScale = MP4GetTrackTimeScale(mp4, textId);
	}

	//We are opened
	opened = true;
	
	return 1;
}

int MP4Streamer::Play()
{
	Log(">MP4Streamer:Play()\n");
	
	//Stop just in case
	Stop();

	//Check we are opened
	if (!opened)
		//Exit
		return Error("-MP4Streamer:Play() | not opened!\n");
	
	//We are playing
	playing = 1;

	//From the begining
	seeked = 0;
	
	//Start event loop
	loop.Start([this](){PlayLoop();});

	Log("<MP4Streamer:Play()\n");

	return playing;
}

int MP4Streamer::PlayLoop()
{
	QWORD audioNext = MP4_INVALID_TIMESTAMP;
	QWORD videoNext = MP4_INVALID_TIMESTAMP;
	QWORD textNext  = MP4_INVALID_TIMESTAMP;

	Log(">MP4Streamer::PlayLoop()\n");

	//If it is from the begining
	if (!seeked)
	{
		// If we have audio
		if (audio)
		{
			//Reset
			audio->Reset();
			// Send audio
			audioNext = audio->GetNextFrameTime();
		}

		// If we have video
		if (video)
		{
			//Reset
			video->Reset();
			// Send video
			videoNext = video->GetNextFrameTime();
		}

		// If we have text
		if (text)
		{
			//Reset
			text->Reset();
			//Get the next frame time
			textNext = text->GetNextFrameTime();
		}
	} else {
		//If we have video
		if (video)
			//Get nearest i frame
			videoNext = video->SeekNearestSyncFrame(seeked);
		//If we have audio
		if (audio)
			//Get nearest frame
			audioNext = audio->Seek(seeked);
		//If we have text
		if (text)
			//Get nearest frame
			textNext = text->Seek(seeked);
	}

	//If first text frame is not sent inmediatelly
	if (text && textNext!=seeked)
		//send previous text subtitle
		text->ReadPrevious(seeked,listener);

	// Calculate start time
	QWORD ini = getTime();

	//Reset time counter
	t = 0;

	// Wait control messages or finish of both streams
	while ( opened && playing && (!(audioNext==MP4_INVALID_TIMESTAMP && videoNext==MP4_INVALID_TIMESTAMP && textNext==MP4_INVALID_TIMESTAMP)))
	{
		// Get next time
		if (audioNext<videoNext)
		{
			if (audioNext<textNext)
				t = audioNext;
			else
				t = textNext;
		} else {
			if (videoNext<textNext)
				t = videoNext;
			else
				t = textNext;
		}

		// Wait time diff
		QWORD now = playbackSpeed * getTimeDiff(ini)/1000;

		if (t>now)
		{
			//Wait next or stopped
			loop.Run(std::chrono::milliseconds(t-now));
			//loop
			continue;
		}

		// if we have to send audio
		if (audioNext<=t)
			audioNext = audio->Read(listener);

		// or video
		if (videoNext<=t)
			videoNext = video->Read(listener);

		// or text
		if (textNext<=t)
			textNext = text->Read(listener);

	}

	Log("-MP4Streamer::PlayLoop()\n");

	//Check if we were stoped
	bool stoped = !opened || !playing;

	//Not playing anymore
	playing = 0;

	//Check end of file
	if (!stoped && listener)
		//End of file
		listener->onEnd();

	Log("<MP4Streamer::PlayLoop()\n");

	return 1;
}

QWORD MP4Streamer::PreSeek(QWORD time)
{
	//Get time
	QWORD seeked = time;

	//If we have video
	if (opened && video)
		//Get nearest i frame
		seeked = video->SeekNearestSyncFrame(time);

	return seeked;
}

int MP4Streamer::Seek(QWORD time)
{
	Log(">MP4Streamer:Seek() [time:%llu]\n",time);

	//Stop Playback
	Stop();

	//Check we are opened
	if (!opened)
	
		//Exit
		return Error("-MP4Streamer:Seek() | not opened!\n");

	//We are playing
	playing = 1;

	//Seet seeked
	seeked = time;

	//Start event loop
	loop.Start([this](){PlayLoop();});

	Log("<MP4Streamer:Seek() | seeked [%lld,%lld]\n",time,seeked);

	return 1;
}

int MP4Streamer::Stop()
{
	//Check
	if (!playing)
		return 0;

	Log(">MP4Streamer::Stop()\n");

	//Change playing state
	playing = 0;

	//Stop loop
	loop.Stop();

	Log("<MP4Streamer::Stop()\n");
	
	return !playing;
}


int MP4Streamer::Close()
{
	Log(">MP4Streamer::Close()\n");
	
	//Check if we wher open
	if (opened)
		return Error("-MP4Streamer::Close() | Not opened");
	
	//Change  state
	opened = false;

	//Stop playback
	Stop();
	
	//If we have been opened
	if (mp4!=MP4_INVALID_FILE_HANDLE)
		// Close file
		MP4Close(mp4);

	//Unset handler
	mp4 = MP4_INVALID_FILE_HANDLE;

	Log("<MP4Streamer::Close()\n");

	return 1;
}

int MP4RtpTrack::SendH264Parameters(Listener *listener)
{
	uint8_t **sequenceHeader;
	uint8_t **pictureHeader;
	uint32_t *pictureHeaderSize;
	uint32_t *sequenceHeaderSize;
	
	uint8_t* data;
	uint32_t len;

	//Not mark
	rtp.SetMark(false);

	// Get SEI information
	MP4GetTrackH264SeqPictHeaders(mp4, track, &sequenceHeader, &sequenceHeaderSize, &pictureHeader, &pictureHeaderSize);
	
	// Get data pointer
	data = rtp.AdquireMediaData();
	// Reset length
	len = 0;

	//Append stap-a header
	data[len++] = 24;
	
	// Check we have sequence header
	if (sequenceHeader)
	{
		uint32_t i = 0;
		// Send sequence headers
		while(sequenceHeader[i] && sequenceHeaderSize[i])
		{
			// Check if it can be handled in a single packeti
			if (sequenceHeaderSize[i]+3<MTU)
			{
				// If there is not enought length
				if (len+sequenceHeaderSize[i]+3>MTU)
				{
					// Set data length
					rtp.SetMediaLength(len);
					//Set seqnum
					rtp.SetSeqNum(seqNum++);
					rtp.Dump();
					//Check listener
					if (listener)
						// Write frame
						listener->onRTPPacket(rtp);
					// Reset data
					len = 0;
					//Append stap-a header
					data[len++] = 24;
				}
				//Set nal size
				set2(data,len,sequenceHeaderSize[i]);
				//Inc len
				len += 2;
				// Copy data
				memcpy(data+len,sequenceHeader[i],sequenceHeaderSize[i]);	
				// Increase pointer
				len+=sequenceHeaderSize[i];
			}
			// Free memory
			free(sequenceHeader[i]);
			// Next header
			i++;
		}
	}

	// Check we have picture header
	if (pictureHeader)
	{
		uint32_t i = 0;
		// Send picture headers
		while(pictureHeader[i] && pictureHeaderSize[i])
		{
			// Check if it can be handled in a single packeti
			if (pictureHeaderSize[i]+3<MTU)
			{
				// If there is not enought length
				if (len+pictureHeaderSize[i]+3>MTU)
				{
					// Set data length
					rtp.SetMediaLength(len);
					//Set seqnum
					rtp.SetSeqNum(seqNum++);
					rtp.Dump();
					//Check listener
					if (listener)
						// Write frame
						listener->onRTPPacket(rtp);
					// Reset data
					len = 0;
					//Append stap-a header
					data[len++] = 24;
				}
				//Set nal size
				set2(data,len,pictureHeaderSize[i]);
				//Inc len
				len += 2;
				// Copy data
				memcpy(data+len,pictureHeader[i],pictureHeaderSize[i]);	
				// Increase pointer
				len+=pictureHeaderSize[i];
			}
			// Free memory
			free(pictureHeader[i]);
			// Next header
			i++;
		}
	}

	// If there is still data
	if (len>1)
	{
		// Set data length
		rtp.SetMediaLength(len);
		//Set seqnum
		rtp.SetSeqNum(seqNum++);
		//Check listenerG44
		if (listener)
			// Write frame
			listener->onRTPPacket(rtp);
		// Reset data
		len = 0;
	}

	// Free data
	if (pictureHeader)
		free(pictureHeader);
	if (sequenceHeader)
		free(sequenceHeader);
	if (sequenceHeaderSize)
		free(sequenceHeaderSize);
	if (pictureHeaderSize)
		free(pictureHeaderSize);
	
	return 1;
}

int MP4RtpTrack::Reset()
{
	Debug("-MP4RtpTrack::Reset()\n");
	
	sampleId	= 1;
	numHintSamples	= 0;
	packetIndex	= 0;
	
	//Reset ssrc on rtp
	rtp.SetSSRC(hint);
	
	return 1;
}

QWORD MP4RtpTrack::Read(Listener *listener)
{
	int last = 0;
	uint8_t* data;
	bool isSyncSample;

	// If it's first packet of a frame
	if (!numHintSamples)
	{
		// Get number of rtp packets for this sample
		if (!MP4ReadRtpHint(mp4, hint, sampleId, &numHintSamples))
		{
			//Print error
			Error("Error reading hintt");
			//Exit
			return MP4_INVALID_TIMESTAMP;
		}

		// Get number of samples for this sample
		frameSamples = MP4GetSampleDuration(mp4, track, sampleId);

		// Get size of sample
		frameSize = MP4GetSampleSize(mp4, track, sampleId);
		// extend buffer for frame size
		if (frame->GetMaxMediaLength() < uint32_t(frameSize))
		{
			frame->Alloc(frameSize);
		}

		// Get sample timestamp
		frameTime = MP4GetSampleTime(mp4, track, sampleId);
		//Convert to miliseconds
		frameTime = MP4ConvertFromTrackTimestamp(mp4, track, frameTime, 1000);

		//Get max data lenght
		BYTE *data = NULL;
		DWORD dataLen = 0;
		MP4Timestamp	startTime;
		MP4Duration	duration;
		MP4Duration	renderingOffset;

		//Get values
		data	= frame->GetData();
		dataLen = frame->GetMaxMediaLength();
		
		// Read next rtp packet
		if (!MP4ReadSample(
			mp4,				// MP4FileHandle hFile
			track,				// MP4TrackId hintTrackId
			sampleId,			// MP4SampleId sampleId,
			(uint8_t **) &data,		// uint8_t** ppBytes
			(uint32_t *) &dataLen,		// uint32_t* pNumBytes
			&startTime,			// MP4Timestamp* pStartTime
			&duration,			// MP4Duration* pDuration
			&renderingOffset,		// MP4Duration* pRenderingOffset
			&isSyncSample			// bool* pIsSyncSample
			))
		{
			Error("Error reading sample [track:%d,sampleId:%d]\n",track,sampleId);
			//Last
			return MP4_INVALID_TIMESTAMP;
		}
		
		UltraDebug("Got frame [time:%d,start:%lu,duration:%lu,lenght:%d,offset:%lu,sinc:%d\n",frameTime,startTime,duration,dataLen,renderingOffset,isSyncSample);
		
		//Check type
		if (media == MediaFrame::Video)
		{
			//Get video frame
			VideoFrame *video = (VideoFrame*)frame;
			//Set lenght
			video->SetLength(dataLen);
			//Set clock rate
			video->SetClockRate(1000);
			//Timestamp
			video->SetTimestamp(startTime);
			//Set intra
			video->SetIntra(isSyncSample);
			//Set video duration (informative)
			video->SetDuration(duration);
		} else {
			//Get Audio frame
			AudioFrame *audio = (AudioFrame*)frame;
			//Set lenght
			audio->SetLength(dataLen);
			//Set clock rate
			audio->SetClockRate(1000);
			//Timestamp
			audio->SetTimestamp(startTime);
			//Set audio duration (informative)
			audio->SetDuration(duration);
		}
		
		//Set rtp timestamp
		rtp.SetExtTimestamp(startTime);
		
		//Set key frame marking
		rtp.SetKeyFrame(MP4GetSampleSync(mp4,track,sampleId));
		
		// Check if it is H264 and it is a Sync frame
		if (codec==VideoCodec::H264 && rtp.IsKeyFrame())
			// Send SPS/PPS info
			SendH264Parameters(listener);
		
		//Check listener
		if (listener)
			//Frame callback
			listener->onMediaFrame(*frame);
	}

	// if it's the last
	if (packetIndex + 1 == numHintSamples)
		//Set last mark
		last = 1;
	
	// Set mark bit
	rtp.SetMark(last);

	// Get data pointer
	data = rtp.AdquireMediaData();
	//Get max data lenght
	DWORD dataLen = rtp.GetMaxMediaLength();

	// Read next rtp packet
	if (!MP4ReadRtpPacket(
				mp4,				// MP4FileHandle hFile
				hint,				// MP4TrackId hintTrackId
				packetIndex++,			// uint16_t packetIndex
				(uint8_t **) &data,		// uint8_t** ppBytes
				(uint32_t *) &dataLen,		// uint32_t* pNumBytes
				0,				// uint32_t ssrc DEFAULT(0)
				0,				// bool includeHeader DEFAULT(true)
				1				// bool includePayload DEFAULT(true)
	))
	{
		//Error
		Error("Error reading packet [%d,%d,%d]\n", hint, track,packetIndex);
		//Exit
		return MP4_INVALID_TIMESTAMP;
	}
		

	//Check
	if (dataLen>rtp.GetMaxMediaLength())
	{
		//Error
		Error("RTP packet too big [%u,%u]\n",dataLen,rtp.GetMaxMediaLength());
		//Exit
		return MP4_INVALID_TIMESTAMP;
	}
	
	//Set media length
	rtp.SetMediaLength(dataLen);
	
	//Set seqnum
	rtp.SetSeqNum(seqNum++);

	// Write frame
	listener->onRTPPacket(rtp);

	// Are we the last packet in a hint?
	if (last)
	{
		// The first hint
		packetIndex = 0;
		// Go for next sample
		sampleId++;
		numHintSamples = 0;
		//Return next frame time
		return GetNextFrameTime();
	}

	// This packet is this one
	return frameTime;
}

QWORD MP4RtpTrack::GetNextFrameTime()
{
	QWORD ts = MP4GetSampleTime(mp4, hint, sampleId);
	//Check it
	if (ts==MP4_INVALID_TIMESTAMP)
		//Return it
		return ts;
	//Convert to miliseconds
	ts = MP4ConvertFromTrackTimestamp(mp4, hint, ts, 1000);

	//Get next timestamp
	return ts;
}


int MP4TextTrack::Reset()
{
	sampleId	= 1;
	return 1;
}

QWORD MP4TextTrack::ReadPrevious(QWORD time,Listener *listener)
{
	//Check it is the first
	if (sampleId==1)
	{
		//Set emtpy frame
		frame.SetFrame(time,(wchar_t*)NULL,0);
		//call listener
		if (listener)
			//Call it
			listener->onTextFrame(frame);
		//Exit
		return 1;
	}

	//The previous one
	MP4SampleId prevId = sampleId-1;

	//If it was not found
	if (sampleId==MP4_INVALID_SAMPLE_ID)
		//The latest
		prevId = MP4GetTrackNumberOfSamples(mp4,track);

	// Get size of sample
	frameSize = MP4GetSampleSize(mp4, track, prevId);

	// Get data pointer
	BYTE *data = (BYTE*)malloc(frameSize);
	//Get max data lenght
	DWORD dataLen = frameSize;

	MP4Timestamp	startTime;
	MP4Duration	duration;
	MP4Duration	renderingOffset;

	// Read next rtp packet
	if (!MP4ReadSample(
				mp4,				// MP4FileHandle hFile
				track,				// MP4TrackId hintTrackId
				prevId,				// MP4SampleId sampleId,
				(uint8_t **) &data,		// uint8_t** ppBytes
				(uint32_t *) &dataLen,		// uint32_t* pNumBytes
				&startTime,			// MP4Timestamp* pStartTime
				&duration,			// MP4Duration* pDuration
				&renderingOffset,		// MP4Duration* pRenderingOffset
				NULL				// bool* pIsSyncSample
	))
		//Last
		return MP4_INVALID_TIMESTAMP;

	//Get length
	if (dataLen>2)
	{
		//Get string length
		DWORD len = data[0]<<8 | data[1];
		//Set frame
		frame.SetFrame(time,data+2+renderingOffset,len-renderingOffset-2);
		//call listener
		if (listener)
			//Call it
			listener->onTextFrame(frame);
	}

	// exit next send time
	return 1;
}

QWORD MP4TextTrack::Read(Listener *listener)
{
	// Get number of samples for this sample
	frameSamples = MP4GetSampleDuration(mp4, track, sampleId);

	// Get size of sample
	frameSize = MP4GetSampleSize(mp4, track, sampleId);

	// Get sample timestamp
	frameTime = MP4GetSampleTime(mp4, track, sampleId);
	//Convert to miliseconds
	frameTime = MP4ConvertFromTrackTimestamp(mp4, track, frameTime, 1000);

	// Get data pointer
	BYTE *data = (BYTE*)malloc(frameSize);
	//Get max data lenght
	DWORD dataLen = frameSize;

	MP4Timestamp	startTime;
	MP4Duration	duration;
	MP4Duration	renderingOffset;

	// Read next rtp packet
	if (!MP4ReadSample(
				mp4,				// MP4FileHandle hFile
				track,				// MP4TrackId hintTrackId
				sampleId++,			// MP4SampleId sampleId,
				(uint8_t **) &data,		// uint8_t** ppBytes
				(uint32_t *) &dataLen,		// uint32_t* pNumBytes
				&startTime,			// MP4Timestamp* pStartTime
				&duration,			// MP4Duration* pDuration
				&renderingOffset,		// MP4Duration* pRenderingOffset
				NULL				// bool* pIsSyncSample
	))
		//Last
		return MP4_INVALID_TIMESTAMP;

	//Log("Got text frame [time:%d,start:%d,duration:%d,lenght:%d,offset:%d\n",frameTime,startTime,duration,dataLen,renderingOffset);
	//Dump(data,dataLen);
	//Get length
	if (dataLen>2)
	{
		//Get string length
		DWORD len = data[0]<<8 | data[1];
		//Set frame
		frame.SetFrame(startTime,data+2+renderingOffset,len-renderingOffset-2);
		//call listener
		if (listener)
			//Call it
			listener->onTextFrame(frame);
	}
	
	// exit next send time
	return GetNextFrameTime();
}

QWORD MP4TextTrack::GetNextFrameTime()
{
	//Get next timestamp
	return  MP4GetSampleTime(mp4, track, sampleId);
}

double MP4Streamer::GetDuration()
{
	auto scale = MP4GetTimeScale(mp4);
	if (scale == 0) return 0.0;
	return MP4GetDuration(mp4)/scale;
}

DWORD MP4Streamer::GetVideoWidth()
{
	return MP4GetTrackVideoWidth(video->mp4,video->track);
}

DWORD MP4Streamer::GetVideoHeight()
{
	return MP4GetTrackVideoHeight(video->mp4,video->track);
}

DWORD MP4Streamer::GetVideoBitrate()
{
	return MP4GetTrackBitRate(video->mp4,video->track);
}

double MP4Streamer::GetVideoFramerate()
{
	return MP4GetTrackVideoFrameRate(video->mp4,video->track);
}

std::unique_ptr<AVCDescriptor> MP4Streamer::GetAVCDescriptor()
{
	uint8_t **sequenceHeader;
	uint8_t **pictureHeader;
	uint32_t *pictureHeaderSize;
	uint32_t *sequenceHeaderSize;
	uint32_t i;
	uint32_t len;
	unsigned char AVCProfileIndication 	= 0x42;	//Baseline
	unsigned char AVCLevelIndication	= 0x0D;	//1.3
	unsigned char AVCProfileCompat		= 0xC0;
			
	//Check video
	if (!video || video->codec!=VideoCodec::H264)
		//Nothing
		return NULL;

	//Create descriptor
	std::unique_ptr<AVCDescriptor> desc = std::make_unique<AVCDescriptor>();

	//Set default
	desc->SetConfigurationVersion(0x01);
	desc->SetAVCProfileIndication(AVCProfileIndication);
	desc->SetProfileCompatibility(AVCProfileCompat);
	desc->SetAVCLevelIndication(AVCLevelIndication);

	//Set nalu length
	MP4GetTrackH264LengthSize(video->mp4, video->track, &len);

	//Set it
	desc->SetNALUnitLengthSizeMinus1(len-1);

	// Get SEI informaMP4GetTrackH264SeqPictHeaderstion
	MP4GetTrackH264SeqPictHeaders(video->mp4, video->track, &sequenceHeader, &sequenceHeaderSize, &pictureHeader, &pictureHeaderSize);

	// Send sequence headers
	i=0;

	// Check we have sequence header
	if (sequenceHeader)
	{
		// Loop array
		while(sequenceHeader[i] && sequenceHeaderSize[i])
		{
			//Append sequence
			desc->AddSequenceParameterSet(sequenceHeader[i],sequenceHeaderSize[i]);
			//Update values based on the ones in SQS
			desc->SetAVCProfileIndication(sequenceHeader[i][1]);
			desc->SetProfileCompatibility(sequenceHeader[i][2]);
			desc->SetAVCLevelIndication(sequenceHeader[i][3]);
			//inc
			i++;
		}
	}

	// Send picture headers
	i=0;

	// Check we have picture header
	if (pictureHeader)
	{
		// Loop array
		while(pictureHeader[i] && pictureHeaderSize[i])
		{
			//Append sequence
			desc->AddPictureParameterSet(pictureHeader[i],pictureHeaderSize[i]);
			//inc
			i++;
		}
	}

	// Free data
	if (pictureHeader)
		free(pictureHeader);
	if (sequenceHeader)
		free(sequenceHeader);
	if (sequenceHeaderSize)
		free(sequenceHeaderSize);
	if (pictureHeaderSize)
		free(pictureHeaderSize);

	return desc;
}


QWORD MP4RtpTrack::SearchNearestSyncFrame(QWORD time)
{
	//Get time in track units
	MP4Duration when = time*timeScale/1000;
	//Get nearest sample
	MP4SampleId sampleId = MP4GetSampleIdFromTime(mp4,hint,when,false);
	//Check
	if (sampleId == MP4_INVALID_SAMPLE_ID)
		//Nothing
		return MP4_INVALID_TIMESTAMP;
	//Find nearest sync
	while(sampleId>1)
	{
		//If it is a sync frame
		if (MP4GetSampleSync(mp4,hint,sampleId)>0)
		{
			//Get sample time
			when = MP4GetSampleTime(mp4,hint,sampleId);
			//And convert it to timescale
			return when*1000/timeScale;
		}
		//new one
		sampleId--;
	}
	//Nothing found go to init
	return MP4_INVALID_TIMESTAMP;
}

QWORD MP4RtpTrack::SeekNearestSyncFrame(QWORD time)
{
	//Reset us
	Reset();
	//Get time in track units
	MP4Duration when = time*timeScale/1000;
	//Get nearest sample
	sampleId = MP4GetSampleIdFromTime(mp4,hint,when,false);
	//Check
	if (sampleId == MP4_INVALID_SAMPLE_ID)
		//Nothing
		return MP4_INVALID_TIMESTAMP;
	//Find nearest sync
	while(sampleId>0)
	{
		//If it is a sync frame
		if (MP4GetSampleSync(mp4,hint,sampleId)>0)
		{
			//Get sample time
			when = MP4GetSampleTime(mp4,hint,sampleId);
			//And convert it to timescale
			return when*1000/timeScale;
		}
		//new one
		sampleId--;
	}
	//Nothing found go to init
	return MP4_INVALID_TIMESTAMP;
}

QWORD MP4RtpTrack::Seek(QWORD time)
{
	//Reset us
	Reset();
	//Get time in track units
	MP4Duration when = time*timeScale/1000;
	//Get nearest sample
	sampleId = MP4GetSampleIdFromTime(mp4,hint,when,false);
	//Check
	if (sampleId == MP4_INVALID_SAMPLE_ID)
		//Nothing
		return MP4_INVALID_TIMESTAMP;
	//Get sample time
	when = MP4GetSampleTime(mp4,hint,sampleId);
	//And convert it to timescale
	return when*1000/timeScale;
}

QWORD MP4TextTrack::Seek(QWORD time)
{
	//Reset us
	Reset();
	//Get time in track units
	MP4Duration when = time*timeScale/1000;
	//Get nearest sample
	sampleId = MP4GetSampleIdFromTime(mp4,track,when,false);
	//Check
	if (sampleId == MP4_INVALID_SAMPLE_ID)
		//Nothing
		return MP4_INVALID_TIMESTAMP;
	//Get sample time
	when = MP4GetSampleTime(mp4,track,sampleId);
	//And convert it to timescale
	return when*1000/timeScale;
}
