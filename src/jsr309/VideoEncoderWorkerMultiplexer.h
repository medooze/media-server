/* 
 * File:   VideoEncoderWorker.h
 * Author: Sergio
 *
 * Created on 2 de noviembre de 2011, 23:37
 */

#ifndef VIDEOENCODERWORKERMULTIPLEXER_H
#define	VIDEOENCODERWORKERMULTIPLEXER_H


#include <pthread.h>
#include "config.h"
#include "codecs.h"
#include "video.h"
#include "core/VideoEncoderWorker.h"
#include "RTPMultiplexerSmoother.h"

class VideoEncoderMultiplexerWorker :
	private VideoEncoderWorker,
	public RTPMultiplexerSmoother
{
public:
	VideoEncoderMultiplexerWorker();
	virtual ~VideoEncoderMultiplexerWorker();
	
	int Init(VideoInput *input);
	int SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties & properties);
	int End();
	
	//Joinable interface
	virtual void AddListener(Listener *listener);
	virtual void Update();
	virtual void SetREMB(int bitrate);
	virtual void RemoveListener(Listener *listener);
	
private:
	int Start();
	int Stop();
	
	virtual void FlushRTXPackets();
	virtual void SmoothFrame(const VideoFrame *videoFrame,DWORD sendingTime);
};

#endif	/* VIDEOENCODERWORKER_H */

