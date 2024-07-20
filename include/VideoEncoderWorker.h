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
#include "acumulator.h"

class VideoEncoderWorker
{
public: 
	struct Stats
	{
		uint64_t timestamp		= 0;
		uint64_t totalEncodedFrames	= 0;
		uint16_t fps			= 0;
		uint32_t bitrate		= 0;
		uint16_t maxEncodingTime	= 0;
		uint16_t avgEncodingTime	= 0;
		uint16_t maxCaptureTime		= 0;
		uint16_t avgCaptureTime		= 0;
		
	};
public:
	VideoEncoderWorker();
	virtual ~VideoEncoderWorker();

	int Init(VideoInput *input);
	int SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties & properties);
	int SetVideoCodec(VideoCodec::Type codec,int width, int height, int fps,int bitrate,int intraPeriod,const Properties & properties);
	int End();

	bool AddListener(const MediaFrame::Listener::shared& listener);
	bool RemoveListener(const MediaFrame::Listener::shared& listener);
	void SendFPU();
	
	bool IsEncoding() { return encoding;	}
	
	int Start();
	int Stop();

	Stats GetStats(); 
	
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

	uint32_t width		= 0;	
	uint32_t height		= 0;
	int fps			= 0;
	DWORD bitrate		= 0;
	int intraPeriod		= 0;
	Properties properties;

	pthread_t	thread = 0;
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	bool	encoding	 = false;
	bool	sendFPU		 = false;


	Acumulator<uint16_t> bitrateAcu;
	Acumulator<uint16_t> fpsAcu;
	MaxAcumulator<uint16_t> encodingTimeAcu;
	MaxAcumulator<uint16_t> capturingTimeAcu;

	Stats	stats;
};


#endif	/* VIDEOENCODERWORKER_H */

