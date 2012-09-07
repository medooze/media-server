/* 
 * File:   rtpparticipant.cpp
 * Author: Sergio
 * 
 * Created on 19 de enero de 2012, 18:41
 */

#include "rtpparticipant.h"

RTPParticipant::RTPParticipant(DWORD partId) : Participant(Participant::RTP,partId) , audio(NULL) , video(this), text(NULL)
{
}

RTPParticipant::~RTPParticipant()
{
}

int RTPParticipant::SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int quality,int fillLevel,int intraPeriod)
{
	//Set it
	return video.SetVideoCodec(codec,mode,fps,bitrate,quality,fillLevel,intraPeriod);
}

int RTPParticipant::SetAudioCodec(AudioCodec::Type codec)
{
	//Set it
	return audio.SetAudioCodec(codec);
}

int RTPParticipant::SetTextCodec(TextCodec::Type codec)
{
	//Set it
	return text.SetTextCodec(codec);
}

int RTPParticipant::SendVideoFPU()
{
	//Send it
	return video.SendFPU();
}

MediaStatistics RTPParticipant::GetStatistics(MediaFrame::Type type)
{
	//Depending on the type
	switch (type)
	{
		case MediaFrame::Audio:
			return audio.GetStatistics();
		case MediaFrame::Video:
			return video.GetStatistics();
		default:
			return text.GetStatistics();
	}
}

int RTPParticipant::End()
{
	int ret = 1;

	//End all streams
	ret &= video.End();
	ret &= audio.End();
	ret &= text.End();
	//aggregater results
	return ret;
}

int RTPParticipant::Init()
{
	int ret = 1;
	//Init each stream
	ret &= video.Init(videoInput,videoOutput);
	ret &= audio.Init(audioInput,audioOutput);
	ret &= text.Init(textInput,textOutput);
	//aggregater results
	return ret;
}

int RTPParticipant::StartSending(MediaFrame::Type media,char *ip, int port,RTPMap& rtpMap)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio.StartSending(ip,port,rtpMap);
		case MediaFrame::Video:
			return video.StartSending(ip,port,rtpMap);
		case MediaFrame::Text:
			return text.StartSending(ip,port,rtpMap);
	}

	return 0;
}

int RTPParticipant::StopSending(MediaFrame::Type media)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio.StopSending();
		case MediaFrame::Video:
			return video.StopSending();
		case MediaFrame::Text:
			return text.StopSending();
	}

	return 0;

}

int RTPParticipant::StartReceiving(MediaFrame::Type media,RTPMap& rtpMap)
{
		switch (media)
	{
		case MediaFrame::Audio:
			return audio.StartReceiving(rtpMap);
		case MediaFrame::Video:
			return video.StartReceiving(rtpMap);
		case MediaFrame::Text:
			return text.StartReceiving(rtpMap);
	}

	return 0;
}

int RTPParticipant::StopReceiving(MediaFrame::Type media)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio.StopReceiving();
		case MediaFrame::Video:
			return video.StopReceiving();
		case MediaFrame::Text:
			return text.StopReceiving();
	}
	return 0;
}

void RTPParticipant::onFPURequested(RTPSession *session)
{
	//Request it
	video.SendFPU();
}

void RTPParticipant::onRequestFPU()
{
	//Check
	if (listener)
		//Call listener
		listener->onRequestFPU(this);
}
int RTPParticipant::SetMute(MediaFrame::Type media, bool isMuted)
{
	//Depending on the type
	switch (media)
	{
		case MediaFrame::Audio:
			// audio
			return audio.SetMute(isMuted);
		case MediaFrame::Video:
			//Attach audio
			return video.SetMute(isMuted);
		case MediaFrame::Text:
			//text
			return text.SetMute(isMuted);
	}
	return 0;
}
