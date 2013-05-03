/* 
 * File:   VideoEncoderWorker.h
 * Author: Sergio
 *
 * Created on 2 de noviembre de 2011, 23:37
 */

#ifndef VIDEOENCODERWORKER_H
#define	VIDEOENCODERWORKER_H


#include <pthread.h>
#include "config.h"
#include "codecs.h"
#include "video.h"
#include "RTPMultiplexerSmoother.h"

class VideoEncoderMultiplexerWorker :
	public RTPMultiplexerSmoother
{
public:
	VideoEncoderMultiplexerWorker();
	virtual ~VideoEncoderMultiplexerWorker();

	int Init(VideoInput *input);
	int SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod);
	int End();
	
	//Joinable interface
	virtual void AddListener(Listener *listener);
	virtual void Update();
	virtual void SetREMB(int bitrate);
	virtual void RemoveListener(Listener *listener);

private:
	int Start();
	int Stop();
protected:
	int Encode();

private:
	static void *startEncoding(void *par);

private:
	VideoInput *input;
	VideoCodec::Type codec;

	int mode;	
	int width;	
	int height;	
	int fps;
	int bitrate;
	int intraPeriod;
	int videoBitrateLimit;
	int videoBitrateLimitCount;

	pthread_t	thread;
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	bool	encoding;
	bool	sendFPU;
};

#endif	/* VIDEOENCODERWORKER_H */

