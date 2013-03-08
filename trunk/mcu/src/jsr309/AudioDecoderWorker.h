/* 
 * File:   AudioDecoderWorker.h
 * Author: Sergio
 *
 * Created on 4 de octubre de 2011, 20:06
 */

#ifndef AUDIODECODERWORKER_H
#define	AUDIODECODERWORKER_H
#include "codecs.h"
#include "audio.h"
#include "waitqueue.h"
#include "Joinable.h"

class AudioDecoderJoinableWorker:
	public Joinable::Listener
{
public:
	AudioDecoderJoinableWorker();
	virtual ~AudioDecoderJoinableWorker();

	int Init(AudioOutput *output);
	int End();

	//Virtuals from Joinable::Listener
	virtual void onRTPPacket(RTPPacket &packet);
	virtual void onResetStream();
	virtual void onEndStream();

	//Attach
	int Attach(Joinable *join);
	int Dettach();

private:
	int Start();
	int Stop();
protected:
	int Decode();

private:
	static void *startDecoding(void *par);

private:
	AudioOutput *output;
	WaitQueue<RTPPacket*> packets;
	pthread_t thread;
	bool decoding;
	Joinable *joined;
	
};

#endif	/* AUDIODECODERWORKER_H */

