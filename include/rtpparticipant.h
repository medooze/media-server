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
	RTPParticipant(DWORD partId,const std::wstring &uuid,const std::wstring &token);
	virtual ~RTPParticipant();

	virtual int SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties& properties);
	virtual int SetAudioCodec(AudioCodec::Type codec,const Properties& properties);
	virtual int SetTextCodec(TextCodec::Type codec);

	virtual int SendVideoFPU();
	virtual MediaStatistics GetStatistics(MediaFrame::Type type);

	virtual int SetVideoInput(VideoInput* input)	{ videoInput	= input; return true;	}
	virtual int SetVideoOutput(VideoOutput* output) { videoOutput	= output; return true;	}
	virtual int SetAudioInput(AudioInput* input)	{ audioInput	= input; return true;	}
	virtual int SetAudioOutput(AudioOutput *output)	{ audioOutput	= output; return true;	}
	virtual int SetTextInput(TextInput* input)	{ textInput	= input; return true;	}
	virtual int SetTextOutput(TextOutput* output)	{ textOutput	= output; return true;	}

	virtual int SetMute(MediaFrame::Type media, bool isMuted);

	virtual int Init();
	virtual int End();

	int StartSending(MediaFrame::Type media,char *sendIp,int sendPort,const RTPMap& rtpMap,const RTPMap& aptMap);
	int StopSending(MediaFrame::Type media);
	int StartReceiving(MediaFrame::Type media,const RTPMap& rtpMap,const RTPMap& aptMap);
	int StopReceiving(MediaFrame::Type media);
	int SetLocalCryptoSDES(MediaFrame::Type media,const char* suite, const char* key64);
	int SetRemoteCryptoSDES(MediaFrame::Type media,const char* suite, const char* key64);
	int SetRemoteCryptoDTLS(MediaFrame::Type media,const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(MediaFrame::Type media,const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(MediaFrame::Type media,const char* username, const char* pwd);
	int SetRTPProperties(MediaFrame::Type media,const Properties& properties);
	
	int SetMediaListener(MediaFrame::Listener *listener) { return video.SetMediaListener(listener); }

	//RTPSession::Listener
	virtual void onFPURequested(RTPSession *session);
	virtual void onReceiverEstimatedMaxBitrate(RTPSession *session,DWORD bitrate);
	virtual void onTempMaxMediaStreamBitrateRequest(RTPSession *session,DWORD bitrate,DWORD overhead);
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

