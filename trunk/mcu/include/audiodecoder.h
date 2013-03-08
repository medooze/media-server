/* 
 * File:   audiodecoder.h
 * Author: Sergio
 *
 * Created on 1 de agosto de 2012, 13:34
 */

#ifndef AUDIODECODER_H
#define	AUDIODECODER_H
#include "codecs.h"
#include "audio.h"
#include "waitqueue.h"
#include "rtp.h"

class AudioDecoderWorker
{
public:
	AudioDecoderWorker();
	virtual ~AudioDecoderWorker();

	int Init(AudioOutput *output);
	int Start();
	void onRTPPacket(RTPPacket &packet);
	int Stop();
	int End();

protected:
	int Decode();

private:
	static void *startDecoding(void *par);

private:
	AudioOutput *output;
	WaitQueue<RTPPacket*> packets;
	pthread_t thread;
	bool decoding;
};

#endif	/* AUDIODECODER_H */

