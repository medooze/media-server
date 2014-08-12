/* 
 * File:   VideoEncoderWorker.h
 * Author: Sergio
 *
 * Created on 12 de agosto de 2014, 10:32
 */

#ifndef VIDEOENCODERWORKER_H
#define	VIDEOENCODERWORKER_H

#include <pthread.h>
#include "config.h"
#include "codecs.h"
#include "video.h"
class VideoEncoderWorker
{
public:
	VideoEncoderWorker();
	virtual ~VideoEncoderWorker();

	int Init(VideoInput *input);
	int SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties & properties);
	int End();

	int  SetTemporalBitrateLimit(int bitrate);
	int  SetMediaListener(MediaFrame::Listener *listener);
	void SendFPU();
	
	bool IsEncoding() { return encoding;	}
	
	virtual void FlushRTXPackets() = 0;
	virtual void SmoothFrame(const VideoFrame *videoFrame,DWORD sendingTime) = 0;

protected:
	int Start();
	int Stop();
	
protected:
	int Encode();

private:
	static void *startEncoding(void *par);

private:
	MediaFrame::Listener *mediaListener;
	
	VideoInput *input;
	VideoCodec::Type codec;

	int mode;	
	int width;	
	int height;	
	int fps;
	int bitrate;
	int intraPeriod;
	int bitrateLimit;
	int bitrateLimitCount;
	Properties properties;

	pthread_t	thread;
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	bool	encoding;
	bool	sendFPU;
};


#endif	/* VIDEOENCODERWORKER_H */

