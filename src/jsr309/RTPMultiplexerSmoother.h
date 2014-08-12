/* 
 * File:   RTPSmoother.h
 * Author: Sergio
 *
 * Created on 7 de noviembre de 2011, 12:18
 */

#ifndef RTPMULTIPLEXERSMOOTHER_H
#define	RTPMULTIPLEXERSMOOTHER_H

#include "config.h"
#include "waitqueue.h"
#include "rtp.h"
#include "RTPMultiplexer.h"


class RTPMultiplexerSmoother :
	public RTPMultiplexer
{
public:
	RTPMultiplexerSmoother();
	virtual ~RTPMultiplexerSmoother();
	int Start();
	int SmoothFrame(const MediaFrame* frame,DWORD duration);
	int Cancel();
	int Wait();
	int Stop();

protected:
	int Run();

private:
	//Funciones propias
	static void *run(void *par);
private:
	pthread_t	thread;
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	bool		inited;
	WaitQueue<RTPPacketSched*> queue;
};

#endif	/* RTPMULTIPLEXERSMOOTHER_H */

