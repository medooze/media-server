#ifndef _AUDIOSTREAM_H_
#define _AUDIOSTREAM_H_

#include <pthread.h>
#include "config.h"
#include "codecs.h"
#include "rtpsession.h"
#include "audio.h"

class AudioStream
{
public:
	AudioStream(RTPSession::Listener* listener);
	~AudioStream();

	int Init(AudioInput *input,AudioOutput *output);
	int SetAudioCodec(AudioCodec::Type codec);
	int StartSending(char* sendAudioIp,int sendAudioPort,RTPMap& rtpMap);
	int StopSending();
	int StartReceiving(RTPMap& rtpMap);
	int StopReceiving();
	int SetMute(bool isMuted);
	int End();

	int IsSending()	  { return sendingAudio;  }
	int IsReceiving() { return receivingAudio;}
	MediaStatistics GetStatistics();

protected:
	int SendAudio();
	int RecAudio();

private:
	//Funciones propias
	static void *startSendingAudio(void *par);
	static void *startReceivingAudio(void *par);

	//Los objectos gordos
	RTPSession	rtp;
	AudioInput	*audioInput;
	AudioOutput	*audioOutput;

	//Parametros del audio
	AudioCodec::Type audioCodec;
	
	//Las threads
	pthread_t 	recAudioThread;
	pthread_t 	sendAudioThread;

	//Controlamos si estamos mandando o no
	volatile int	sendingAudio;
	volatile int 	receivingAudio;

	bool		muted;
};
#endif
