#ifndef VIDEODECODERWORKER_H
#define VIDEODECODERWORKER_H

#include "codecs.h"
#include "video.h"
#include "waitqueue.h"
#include "rtp.h"
#include "Deinterlacer.h"

class VideoDecoderWorker 
	: public MediaFrame::Listener
{
public:
	VideoDecoderWorker() = default;
	virtual ~VideoDecoderWorker();

	int Start();
	int Stop();

	// MediaFrame::Listener interface
	virtual void onMediaFrame(const MediaFrame& frame) override;
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame& frame)  override { onMediaFrame(frame); }
	
	void AddVideoOutput(VideoOutput* ouput);
	void RemoveVideoOutput(VideoOutput* ouput);

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
};

#endif /* VIDEODECODERWORKER_H */

