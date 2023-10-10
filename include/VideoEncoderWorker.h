/* 
 * File:   VideoEncoderWorker.h
 * Author: Sergio
 *
 * Created on 12 de agosto de 2014, 10:32
 */

#ifndef VIDEOENCODERWORKER_H
#define	VIDEOENCODERWORKER_H

#include <pthread.h>
#include <set>
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
	int SetVideoCodec(VideoCodec::Type codec,int width, int height, int fps,int bitrate,int intraPeriod,const Properties & properties);
	int End();

	int  SetTemporalBitrateLimit(int bitrate);
	bool AddListener(const MediaFrame::Listener::shared& listener);
	bool RemoveListener(const MediaFrame::Listener::shared& listener);
	void SendFPU();
	
	bool IsEncoding() { return encoding;	}
	
	int Start();
	int Stop();
	
protected:
	int Encode();

private:
	static void *startEncoding(void *par);

private:
	typedef std::set<MediaFrame::Listener::shared> Listeners;
	
private:
	Listeners		listeners;
	
	VideoInput *input	= nullptr;
	VideoCodec::Type codec  = VideoCodec::UNKNOWN;

	int width		= 0;	
	int height		= 0;	
	int fps			= 0;
	DWORD bitrate		= 0;
	int intraPeriod		= 0;
	int bitrateLimit	= 0;
	int bitrateLimitCount	= 0;
	Properties properties;

	pthread_t	thread = 0;
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	bool	encoding	 = false;
	bool	sendFPU		 = false;
};


#endif	/* VIDEOENCODERWORKER_H */

