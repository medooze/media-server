/* 
 * File:   AudioEncoderWorker.h
 * Author: Sergio
 *
 * Created on 4 de octubre de 2011, 20:42
 */

#ifndef AUDIOENCODERWORKER_H
#define	AUDIOENCODERWORKER_H
#include "codecs.h"
#include "audio.h"
#include "RTPMultiplexer.h"


class AudioEncoderMultiplexerWorker :
	public RTPMultiplexer
{
public:
	AudioEncoderMultiplexerWorker();
	virtual ~AudioEncoderMultiplexerWorker();

	int Init(AudioInput *input);
	int SetCodec(AudioCodec::Type codec);
	int End();
	//Joinable interface
	virtual void AddListener(Listener *listener);
	virtual void Update();
	virtual void RemoveListener(Listener *listener);

private:
	int Start();
	int Stop();
protected:
	int Encode();
	
private:
	static void *startEncoding(void *par);

private:
	AudioInput *input;
	AudioCodec::Type codec;
	pthread_t thread;
	bool encoding;
};

#endif	/* AUDIOENCODERWORKER_H */

