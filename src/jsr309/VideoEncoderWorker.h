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
	int SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int qMin=0, int qMax=0,int intraPeriod=0);
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
	VideoInput *input;
	VideoCodec::Type codec;

	int mode;	//Modo de captura de video actual
	int width;	//Ancho de la captura
	int height;	//Alto de la captur
	int fps;
	int bitrate;
	int qMin;
	int qMax;
	int intraPeriod;

	pthread_t	thread;
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	bool	encoding;
	bool	sendFPU;
};

#endif	/* VIDEOENCODERWORKER_H */

