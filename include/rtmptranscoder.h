#ifndef _RTMPTRANSCODER_H_
#define _RTMPTRANSCODER_H_
#include "config.h"
#include "rtmpbroadcaster.h"

class RTMPTranscoder : 
	public RTMPBroadcaster
{

	int Init();
	int End();

	//Video
	int SetVideoCodec(int codec,int mode,int fps,int bitrate,int quality=0, int fillLevel=0);
	int StartSendingVideo(,char *sendVideoIp,int sendVideoPort);
	int StopSendingVideo();
	int IsSendingVideo();

	//Audio
	int SetAudioCodec(int codec);
	int StartSendingAudio(,char *sendAudioIp,int sendAudioPort);
	int StopSendingAudio();
	int IsSendingAudio();

private:
	VideoEncoder* videoEncoder;
	AudioEncoder* audioEncoder;

privatePipeVideoOutput *videoOutput;
	PipeAudioOutput *audioOutput;
};
