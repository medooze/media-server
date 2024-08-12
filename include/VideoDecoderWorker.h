#ifndef VIDEODECODERWORKER_H
#define VIDEODECODERWORKER_H

#include "codecs.h"
#include "video.h"
#include "waitqueue.h"
#include "rtp.h"
#include "Deinterlacer.h"
#include "acumulator.h"

class VideoDecoderWorker 
	: public MediaFrame::Listener
{
public:
	struct Stats
	{
		uint64_t timestamp		= 0;
		uint64_t totalDecodedFrames	= 0;
		uint16_t fps			= 0;
		uint16_t maxDecodingTime	= 0;
		uint16_t avgDecodingTime	= 0;
		uint16_t maxWaitingFrameTime	= 0;
		uint16_t avgWaitingFrameTime	= 0;
		uint16_t maxDeinterlacingTime	= 0;
		uint16_t avgDeinterlacingTime	= 0;
	};
public:
	VideoDecoderWorker();
	virtual ~VideoDecoderWorker();

	int Start();
	int Stop();

	// MediaFrame::Listener interface
	virtual void onMediaFrame(const MediaFrame& frame) override;
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame& frame)  override { onMediaFrame(frame); }
	
	void AddVideoOutput(VideoOutput* ouput);
	void RemoveVideoOutput(VideoOutput* ouput);

	Stats GetStats();
protected:
	int Decode();

private:
	static void *startDecoding(void *par);

private:
	std::set<VideoOutput*> outputs;
	WaitQueue<std::shared_ptr<VideoFrame>> frames;
	pthread_t thread = 0;
	Mutex mutex;
	bool decoding	= false;
	bool muted	= false;
	std::unique_ptr<VideoDecoder>	videoDecoder;
	std::unique_ptr<Deinterlacer>	deinterlacer;

	Stats stats;
	Acumulator<uint16_t> bitrateAcu;
	Acumulator<uint16_t> fpsAcu;
	MaxAcumulator<uint16_t> decodingTimeAcu;
	MaxAcumulator<uint16_t> waitingFrameTimeAcu;
	MaxAcumulator<uint16_t> deinterlacingTimeAcu;
};

#endif /* VIDEODECODERWORKER_H */

