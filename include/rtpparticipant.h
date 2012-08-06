/* 
 * File:   rtpparticipant.h
 * Author: Sergio
 *
 * Created on 19 de enero de 2012, 18:41
 */

#ifndef RTPPARTICIPANT_H
#define	RTPPARTICIPANT_H

#include "config.h"
#include "participant.h"
#include "videostream.h"
#include "audiostream.h"
#include "textstream.h"
#include "mp4recorder.h"

class RTPParticipant : public Participant, public VideoStream::Listener
{
public:
	RTPParticipant(DWORD partId);
	virtual ~RTPParticipant();

	virtual int SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int quality=0, int fillLevel=0,int intraPeriod=0);
	virtual int SetAudioCodec(AudioCodec::Type codec);
	virtual int SetTextCodec(TextCodec::Type codec);

	virtual int SendVideoFPU();
	virtual MediaStatistics GetStatistics(MediaFrame::Type type);

	virtual int SetVideoInput(VideoInput* input)	{ videoInput	= input;	}
	virtual int SetVideoOutput(VideoOutput* output) { videoOutput	= output;	}
	virtual int SetAudioInput(AudioInput* input)	{ audioInput	= input;	}
	virtual int SetAudioOutput(AudioOutput *output)	{ audioOutput	= output;	}
	virtual int SetTextInput(TextInput* input)	{ textInput	= input;	}
	virtual int SetTextOutput(TextOutput* output)	{ textOutput	= output;	}

	virtual int SetMute(MediaFrame::Type media, bool isMuted);

	virtual int Init();
	virtual int End();

	int StartSendingVideo(char *sendVideoIp,int sendVideoPort,VideoCodec::RTPMap& rtpMap);
	int StopSendingVideo();
	int StartReceivingVideo(VideoCodec::RTPMap& rtpMap);
	int StopReceivingVideo();
	int StartSendingAudio(char *sendAudioIp,int sendAudioPort,AudioCodec::RTPMap& rtpMap);
	int StopSendingAudio();
	int StartReceivingAudio(AudioCodec::RTPMap& rtpMap);
	int StopReceivingAudio();
	int StartSendingText(char *sendTextIp,int sendTextPort,TextCodec::RTPMap& rtpMap);
	int StopSendingText();
	int StartReceivingText(TextCodec::RTPMap& rtpMap);
	int StopReceivingText();

	int SetMediaListener(MediaFrame::Listener *listener) { return video.SetMediaListener(listener); }

	//RTPSession::Listener
	virtual void onFPURequested(RTPSession *session);
	virtual void onRequestFPU();
public:
	MP4Recorder	recorder; //FIX this!
private:
	VideoStream	video;
	AudioStream	audio;
	TextStream	text;
	

	VideoInput*	videoInput;
	VideoOutput*	videoOutput;
	AudioInput*	audioInput;
	AudioOutput*	audioOutput;
	TextInput*	textInput;
	TextOutput*	textOutput;
};

#endif	/* RTPPARTICIPANT_H */

