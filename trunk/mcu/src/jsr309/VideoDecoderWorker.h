/* 
 * File:   VideoDecoderWorker.h
 * Author: Sergio
 *
 * Created on 2 de noviembre de 2011, 23:38
 */

#ifndef VIDEODECODERWORKER_H
#define	VIDEODECODERWORKER_H

#include "codecs.h"
#include "video.h"
#include "waitqueue.h"
#include "Joinable.h"

class VideoDecoderJoinableWorker:
	public Joinable::Listener
{
public:
	VideoDecoderJoinableWorker();
	virtual ~VideoDecoderJoinableWorker();

	int Init(VideoOutput *output);
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
	VideoOutput *output;
	WaitQueue<RTPPacket*> packets;
	pthread_t thread;
	bool decoding;
	Joinable *joined;
};

#endif	/* VIDEODECODERWORKER_H */

