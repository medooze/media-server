#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "log.h"
#include "codecs.h"
#include "rtp.h"
#include "mp4streamer.h"
#include "video.h"
#include "audio.h"


MP4Streamer::MP4Streamer(Listener *listener)
{
	//Save listener
	this->listener = listener;
	//NO file
	mp4 = MP4_INVALID_FILE_HANDLE;
	//Not playing
	playing = false;
	//Not opened
	opened = false;
	//No tracks
	audio = NULL;
	video = NULL;
	text = NULL;
	//No time or seeked
	seeked = 0;
	t = 0;
	//Inciamos lso mutex y la condicion
	pthread_mutex_init(&mutex,0);
	pthread_cond_init(&cond,0);
	//Clean thread
	setZeroThread(&thread);
}

MP4Streamer::~MP4Streamer()
{
	//Check if still opened
	if (opened)
		//Close us
		Close();
	
	//Check tracks and delete
	if (audio)
		delete (audio);
	if (video)
		delete (video);
	if (text)
		delete (text);
	//Liberamos los mutex
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

int MP4Streamer::Open(const char *filename)
{
	//LOg
	Log(">MP4 opening [%s]\n",filename);

	//Lock
	pthread_mutex_lock(&mutex);

	//If already opened
	if (opened)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Return error
		return Error("Already opened\n");
	}
	
	// Open mp4 file
	mp4 = MP4Read(filename);

	// If not valid
	if (mp4 == MP4_INVALID_FILE_HANDLE)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Return error
		return Error("Invalid file handle for %s\n",filename);
	}
	
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

		Log("-Found hint track [hintId:%d]\n", hintId);

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

			Log("-Streaming media [trackId:%d,type:\"%s\",name:\"%s\",payload:%d]\n", trackId, type, name, payload);

			// Check track type
			if ((strcmp(type, MP4_AUDIO_TRACK_TYPE) == 0) && !audio)
			{
				// Depending on the name
				if (strcmp("PCMU", name) == 0)
					//Create new audio track
					audio = new MP4RtpTrack(MediaFrame::Audio,AudioCodec::PCMU,payload);
				else if (strcmp("PCMA", name) == 0)
					//Create new audio track
					audio = new MP4RtpTrack(MediaFrame::Audio,AudioCodec::PCMA,payload);
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
				// Depending on the name
				if (strcmp("H263", name) == 0)
					//Create new video track
					video = new MP4RtpTrack(MediaFrame::Video,VideoCodec::H263_1996,payload);
				else if (strcmp("H263-1998", name) == 0)
					//Create new video track
					video = new MP4RtpTrack(MediaFrame::Video,VideoCodec::H263_1998,payload);
				else if (strcmp("H263-2000", name) == 0)
					//Create new video track
					video = new MP4RtpTrack(MediaFrame::Video,VideoCodec::H263_1998,payload);
				else if (strcmp("H264", name) == 0)
					//Create new video track
					video = new MP4RtpTrack(MediaFrame::Video,VideoCodec::H264,payload);
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

	Log("-Found text track [%d]\n",textId);

	// Iterate hint tracks
	if (textId != MP4_INVALID_TRACK_ID)
	{
		//We have it
		text = new MP4TextTrack();
		//Set values
		text->mp4 = mp4;
		text->track = textId;
		text->sampleId = 1;
		// Get time scale
		text->timeScale = MP4GetTrackTimeScale(mp4, textId);
	}

	//We are opened
	opened = true;

	//Unlock
	pthread_mutex_unlock(&mutex);
	
	return 1;
}

int MP4Streamer::Play()
{
	Log(">MP4Streamer Play\n");
	
	//Stop just in case
	Stop();

	//Lock
	pthread_mutex_lock(&mutex);

	//Check we are opened
	if (!opened)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Exit
		return Error("MP4Streamer not opened!\n");
	}
	
	//We are playing
	playing = 1;

	//From the begining
	seeked = 0;
	
	//Arrancamos los procesos
	createPriorityThread(&thread,play,this,0);

	//Unlock
	pthread_mutex_unlock(&mutex);

	Log("<MP4Streamer Play\n");

	return playing;
}

void * MP4Streamer::play(void *par)
{
        Log("-PlayThread [%p]\n",pthread_self());

	//Obtenemos el parametro
	MP4Streamer *player = (MP4Streamer *)par;

	//Bloqueamos las se�ales
	blocksignals();

	//Ejecutamos
	player->PlayLoop();
	//Exit
	return NULL;
}

int MP4Streamer::PlayLoop()
{
	QWORD audioNext = MP4_INVALID_TIMESTAMP;
	QWORD videoNext = MP4_INVALID_TIMESTAMP;
	QWORD textNext  = MP4_INVALID_TIMESTAMP;
	timeval tv ;
	timespec ts;

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
	getUpdDifTime(&tv);

	//Lock
	pthread_mutex_lock(&mutex);

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
		QWORD now = (QWORD)getDifTime(&tv)/1000+seeked;

		if (t>now)
		{
			//Calculate timeout
			calcAbsTimeout(&ts,&tv,t-seeked);

			//Wait next or stopped
			pthread_cond_timedwait(&cond,&mutex,&ts);
			
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

	//Unlock
	pthread_mutex_unlock(&mutex);

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

	//Lock
	pthread_mutex_lock(&mutex);

	//If we have video
	if (opened && video)
		//Get nearest i frame
		seeked = video->SeekNearestSyncFrame(time);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return seeked;
}

int MP4Streamer::Seek(QWORD time)
{
	Log(">MP4Streamer seek\n");

	//Stop Playback
	Stop();

	//Lock
	pthread_mutex_lock(&mutex);

	//Check we are opened
	if (!opened)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Exit
		return Error("MP4Streamer not opened!\n");
	}

	//We are playing
	playing = 1;

	//Seet seeked
	seeked = time;

	//Arrancamos los procesos
	createPriorityThread(&thread,play,this,0);

	//Unlock
	pthread_mutex_unlock(&mutex);

	Log("<MP4Streamer seeked [%lld,%lld]\n",time,seeked);

	return 1;
}

int MP4Streamer::Stop()
{
	//Check
	if (!playing)
		return 0;

	Log(">MP4Streamer Stop\n");

	//Lock
	pthread_mutex_lock(&mutex);

	//Change playing state
	playing = 0;

	//Get running thread
	pthread_t running = thread;

	//Clean thread
	setZeroThread(&thread);

	//Signal
	pthread_cond_signal(&cond);

	//Unlock
	pthread_mutex_unlock(&mutex);

	//If got thread
	if (running)
		//Wait for thread, will return EDEADLK if called from same thread, i.e. in from onEnd
		pthread_join(running,NULL);


	Log("<MP4Streamer stop\n");
	
	return !playing;
}


int MP4Streamer::Close()
{
	Log(">MP4 Close\n");
	
	//Lock
	pthread_mutex_lock(&mutex);

	//Change  state
	opened = false;

	//If we have been opened
	if (mp4!=MP4_INVALID_FILE_HANDLE)
		// Close file
		MP4Close(mp4);

	//Unset handler
	mp4 = MP4_INVALID_FILE_HANDLE;

	//It it waas playing
	if (playing)
	{
		//Not playing
		playing = 0;

		//Signal
		pthread_cond_signal(&cond);

		//Get running thread
		DWORD running = thread;

		//Clean thread
		setZeroThread(&thread);

		//Unlock
		pthread_mutex_unlock(&mutex);

		//Check thread
		if (running)
			//Wait for running thread
			pthread_join(running,NULL);
	} else {
		//Unlock
		pthread_mutex_unlock(&mutex);
	}

	Log("<MP4 Close\n");

	return 1;
}

int MP4RtpTrack::SendH263SEI(Listener *listener)
{
	uint8_t **sequenceHeader;
	uint8_t **pictureHeader;
	uint32_t *pictureHeaderSize;
	uint32_t *sequenceHeaderSize;
	uint32_t i;
	uint8_t* data;
	uint32_t dataLen;

	//Not mark
	rtp.SetMark(false);

	// Get SEI information
	MP4GetTrackH264SeqPictHeaders(mp4, track, &sequenceHeader, &sequenceHeaderSize, &pictureHeader, &pictureHeaderSize);

	// Get data pointer
	data = rtp.GetMediaData();
	// Reset length
	dataLen = 0;

	// Send sequence headers
	i=0;

	// Check we have sequence header
	if (sequenceHeader)
		// Loop array
		while(sequenceHeader[i] && sequenceHeaderSize[i])
		{
			// Check if it can be handled in a single packeti
			if (sequenceHeaderSize[i]<1400)
			{
				// If there is not enought length
				if (dataLen+sequenceHeaderSize[i]>1400)
				{
					// Set data length
					rtp.SetMediaLength(dataLen);
					//Check listener
					if (listener)
						// Write frame
						listener->onRTPPacket(rtp);
					// Reset data
					dataLen = 0;
				}
				// Copy data
				memcpy(data+dataLen,sequenceHeader[i],sequenceHeaderSize[i]);	
				// Increase pointer
				dataLen+=sequenceHeaderSize[i];
			}
			// Free memory
			free(sequenceHeader[i]);
			// Next header
			i++;
		}

	// If there is still data
	if (dataLen>0)
	{
		// Set data length
		rtp.SetMediaLength(dataLen);
		//Check listener
		if (listener)
			// Write frame
			listener->onRTPPacket(rtp);
		// Reset data
		dataLen = 0;
	}

	// Send picture headers
	i=0;

	// Check we have picture header
	if (pictureHeader)
		// Loop array
		while(pictureHeader[i] && pictureHeaderSize[i])
		{
			// Check if it can be handled in a single packeti
			if (pictureHeaderSize[i]<1400)
			{
				// If there is not enought length
				if (dataLen+pictureHeaderSize[i]>1400)
				{
					// Set data length
					rtp.SetMediaLength(dataLen);
					//Check listener
					if (listener)
						// Write frame
						listener->onRTPPacket(rtp);
					// Reset data
					dataLen = 0;
				}
				// Copy data
				memcpy(data+dataLen,pictureHeader[i],pictureHeaderSize[i]);	
				// Increase pointer
				dataLen+=pictureHeaderSize[i];
			}
			// Free memory
			free(pictureHeader[i]);
			// Next header
			i++;
		}

	// If there is still data
	if (dataLen>0)
	{
		// Set data length
		rtp.SetMediaLength(dataLen);
		//Check listener
		if (listener)
			// Write frame
			listener->onRTPPacket(rtp);
		// Reset data
		dataLen = 0;
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

}

int MP4RtpTrack::Reset()
{
	sampleId	= 1;
	numHintSamples	= 0;
	packetIndex	= 0;
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
		frameSamples = MP4GetSampleDuration(mp4, hint, sampleId);

		// Get size of sample
		frameSize = MP4GetSampleSize(mp4, hint, sampleId);

		// Get sample timestamp
		frameTime = MP4GetSampleTime(mp4, hint, sampleId);
		//Convert to miliseconds
		frameTime = MP4ConvertFromTrackTimestamp(mp4, hint, frameTime, 1000);

		// Check if it is H264 and it is a Sync frame
		if (codec==VideoCodec::H264 && MP4GetSampleSync(mp4,track,sampleId))
			// Send SEI info
			SendH263SEI(listener);

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
			(u_int8_t **) &data,		// u_int8_t** ppBytes
			(u_int32_t *) &dataLen,		// u_int32_t* pNumBytes
			&startTime,			// MP4Timestamp* pStartTime
			&duration,			// MP4Duration* pDuration
			&renderingOffset,		// MP4Duration* pRenderingOffset
			&isSyncSample			// bool* pIsSyncSample
			))
		{
			Error("Error reading sample");
			//Last
			return MP4_INVALID_TIMESTAMP;
		}

		//Check type
		if (media == MediaFrame::Video)
		{
			//Get video frame
			VideoFrame *video = (VideoFrame*)frame;
			//Set lenght
			video->SetLength(dataLen);
			//Timestamp
			video->SetTimestamp(startTime*90000/timeScale);
			//Set intra
			video->SetIntra(isSyncSample);
			//Set video duration (informative)
			video->SetDuration(duration);
		} else {
			//Get Audio frame
			AudioFrame *audio = (AudioFrame*)frame;
			//Set lenght
			audio->SetLength(dataLen);
			//Timestamp
			audio->SetTimestamp(startTime*8000/timeScale);
			//Set audio duration (informative)
			audio->SetDuration(duration);
		}

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
	data = rtp.GetMediaData();
	//Get max data lenght
	DWORD dataLen = rtp.GetMaxMediaLength();

	// Read next rtp packet
	if (!MP4ReadRtpPacket(
				mp4,				// MP4FileHandle hFile
				hint,				// MP4TrackId hintTrackId
				packetIndex++,			// u_int16_t packetIndex
				(u_int8_t **) &data,		// u_int8_t** ppBytes
				(u_int32_t *) &dataLen,		// u_int32_t* pNumBytes
				0,				// u_int32_t ssrc DEFAULT(0)
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
	
	//Set lenght
	rtp.SetMediaLength(dataLen);
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
				(u_int8_t **) &data,		// u_int8_t** ppBytes
				(u_int32_t *) &dataLen,		// u_int32_t* pNumBytes
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
	int next = 0;
	int last = 0;
	int first = 0;

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
				(u_int8_t **) &data,		// u_int8_t** ppBytes
				(u_int32_t *) &dataLen,		// u_int32_t* pNumBytes
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
	return MP4GetDuration(mp4)/MP4GetTimeScale(mp4);
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

AVCDescriptor* MP4Streamer::GetAVCDescriptor()
{
	uint8_t **sequenceHeader;
	uint8_t **pictureHeader;
	uint32_t *pictureHeaderSize;
	uint32_t *sequenceHeaderSize;
	uint32_t i;
	uint8_t profile, level;
	uint32_t len;
	
	//Check video
	if (!video || video->codec!=VideoCodec::H264)
		//Nothing
		return NULL;

	//Create descriptor
	AVCDescriptor* desc = new AVCDescriptor();

	//Set them
	desc->SetConfigurationVersion(0x01);
	desc->SetAVCProfileIndication(profile);
	desc->SetProfileCompatibility(0x00);
	desc->SetAVCLevelIndication(level);

	//Set nalu length
	MP4GetTrackH264LengthSize(video->mp4, video->track, &len);

	//Set it
	desc->SetNALUnitLength(len-1);

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
