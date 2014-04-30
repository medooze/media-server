/* 
 * File:   rtmpparticipant.h
 * Author: Sergio
 *
 * Created on 19 de enero de 2012, 18:43
 */

#ifndef RTMPPARTICIPANT_H
#define	RTMPPARTICIPANT_H

#include "rtmpstream.h"
#include "codecs.h"
#include "participant.h"
#include "multiconf.h"
#include "waitqueue.h"

class RTMPParticipant :
	public Participant,
	public RTMPMediaStream,
	public RTMPMediaStream::Listener
{
public:
	RTMPParticipant(DWORD partId,const std::wstring &token);
	virtual ~RTMPParticipant();

	virtual int SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties& properties);
	virtual int SetAudioCodec(AudioCodec::Type codec,const Properties& properties);
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

	/* Overrride from RTMPMediaStream*/
	virtual DWORD AddMediaListener(RTMPMediaStream::Listener *listener);
	virtual DWORD RemoveMediaListener(RTMPMediaStream::Listener *listener);
	virtual void RemoveAllMediaListeners();

	//RTMPMediaStream Listener
	virtual void onAttached(RTMPMediaStream *stream);
	virtual void onMediaFrame(DWORD id,RTMPMediaFrame *frame);
	virtual void onMetaData(DWORD id,RTMPMetaData *meta);
	virtual void onCommand(DWORD id,const wchar_t *name,AMFData* obj);
	virtual void onStreamBegin(DWORD id);
	virtual void onStreamEnd(DWORD id);
	virtual void onStreamReset(DWORD id);
	virtual void onDetached(RTMPMediaStream *stream);

protected:
	int RecVideo();
	int RecAudio();
	int SendText();
	int SendVideo();
	int SendAudio();

private:
	bool StartSending();
	void StopSending();
	bool StartReceiving();
	void StopReceiving();
	//Video
	int StartSendingVideo();
	int SetSendingVideoCodec();
	int SendFPU();
	int StopSendingVideo();
	int StartReceivingVideo();
	int StopReceivingVideo();
	//Audio RTP
	int StartSendingAudio();
	int SetSendingAudioCodec();
	int StopSendingAudio();
	int StartReceivingAudio();
	int StopReceivingAudio();
	//T140 Text RTP
	int StartSendingText();
	int StopSendingText();
	int StartReceivingText();
	int StopReceivingText();

	static void* startReceivingVideo(void *par);
	static void* startReceivingAudio(void *par);

	static void* startSendingVideo(void *par);
	static void* startSendingAudio(void *par);
	static void* startSendingText(void *par);
private:
	RTMPMediaStream		*attached;
	RTMPMetaData		*meta;
	RTMPVideoFrame*		frameDesc;
	RTMPAudioFrame*		aacSpecificConfig;
	MediaStatistics		audioStats;
	MediaStatistics		videoStats;
	MediaStatistics		textStats;
	WaitQueue<RTMPVideoFrame*> videoFrames;
	WaitQueue<RTMPAudioFrame*> audioFrames;

	AudioCodec::Type audioCodec;
	Properties	 audioProperties;
	VideoCodec::Type videoCodec;
	int 		videoWidth;
	int 		videoHeight;
	int 		videoFPS;
	int 		videoBitrate;
	int		videoIntraPeriod;
	Properties	videoProperties;

	//Las threads
	pthread_t 	recVideoThread;
	pthread_t 	recAudioThread;
	pthread_t 	sendTextThread;
	pthread_t 	sendVideoThread;
	pthread_t 	sendAudioThread;
	pthread_mutex_t	mutex;
	pthread_cond_t	cond;

	//Controlamos si estamos mandando o no
	bool	sendingVideo;
	bool 	receivingVideo;
	bool	sendingAudio;
	bool	receivingAudio;
	bool	sendingText;
	bool	inited;
	bool	sendFPU;
	timeval	first;

	VideoInput*	videoInput;
	VideoOutput*	videoOutput;
	AudioInput*	audioInput;
	AudioOutput*	audioOutput;
	TextInput*	textInput;
	TextOutput*	textOutput;

	bool	audioMuted;
	bool	videoMuted;
	bool	textMuted;
};

#endif	/* RTMPPARTICIPANT_H */

