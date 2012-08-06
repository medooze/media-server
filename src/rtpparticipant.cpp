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
	//End all streams
	video.End();
	audio.End();
	text.End();
}

int RTPParticipant::Init()
{
	//Init each stream
	video.Init(videoInput,videoOutput);
	audio.Init(audioInput,audioOutput);
	text.Init(textInput,textOutput);
}

int RTPParticipant::StartSendingVideo(char *ip,int port,VideoCodec::RTPMap& rtpMap)
{
	//Start sending
	return video.StartSending(ip,port,rtpMap);
}

int RTPParticipant::StopSendingVideo()
{
	//Stop sending
	return video.StopSending();
}

int RTPParticipant::StartReceivingVideo(VideoCodec::RTPMap& rtpMap)
{
	//Start receiving
	return video.StartReceiving(rtpMap);
}

int RTPParticipant::StopReceivingVideo()
{
	//Stop receiving
	return video.StopReceiving();
}

int RTPParticipant::StartSendingAudio(char *ip,int port,AudioCodec::RTPMap& rtpMap)
{
	//Start sending
	return audio.StartSending(ip,port,rtpMap);
}

int RTPParticipant::StopSendingAudio()
{
	//Stop sending
	return audio.StopSending();
}

int RTPParticipant::StartReceivingAudio(AudioCodec::RTPMap& rtpMap)
{
	//Start receiving
	return audio.StartReceiving(rtpMap);
}

int RTPParticipant::StopReceivingAudio()
{
	//Stop receiving
	return audio.StopReceiving();
}

int RTPParticipant::StartSendingText(char *ip,int port,TextCodec::RTPMap& rtpMap)
{
	//Start sending
	return text.StartSending(ip,port,rtpMap);
}

int RTPParticipant::StopSendingText()
{
	//Stop sending
	return text.StopSending();

}

int RTPParticipant::StartReceivingText(TextCodec::RTPMap& rtpMap)
{
	//Start receiving
	return text.StartReceiving(rtpMap);
}

int RTPParticipant::StopReceivingText()
{
	//Stop receiving
	return text.StopReceiving();
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
