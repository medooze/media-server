#ifndef VIDEOPIPE_H
#define	VIDEOPIPE_H

#include <pthread.h>
#include "video.h"
#include "VideoBufferScaler.h"
#include "CircularQueue.h"

class VideoPipe :
	public VideoOutput,
	public VideoInput
{
public:
	enum AllowedDownScaling
	{
		Any		= 0,
		SameOrLower	= 1,
		LowerOnly	= 2
	};
public:
	VideoPipe();
	~VideoPipe() override;

	int Init(float scaleResolutionDownBy = 0.0f, uint32_t scaleResolutionToHeight = 0, AllowedDownScaling allowedDownScaling = AllowedDownScaling::Any);
	int End();

	/** VideoInput */
	int StartVideoCapture(uint32_t width, uint32_t height, uint32_t fps) override;
	VideoBuffer::const_shared GrabFrame(uint32_t timeout) override;
	void CancelGrabFrame() override;
	int StopVideoCapture() override;

	/** VideoOutput */
	size_t NextFrame(const VideoBuffer::const_shared& videoBuffer) override;
	void ClearFrame() override;

private:
	uint32_t videoWidth = 0;
	uint32_t videoHeight = 0;
	int videoFPS = 0;
	float scaleResolutionDownBy = 0.0f;
	uint32_t scaleResolutionToHeight = 0;
	int inited = false;
	int capturing = false;
	int cancelledGrab = false;

	CircularQueue<VideoBuffer::const_shared> queue;

	pthread_mutex_t newPicMutex;
	pthread_cond_t  newPicCond;

	VideoBufferPool	videoBufferPool;
	VideoBufferScaler scaler;
	AllowedDownScaling allowedDownScaling = AllowedDownScaling::Any;

	uint64_t lastDroppedReport = 0;
	unsigned int droppedFramesSinceReport = 0;
	unsigned int totalFramesSinceReport = 0;
};

#endif	/* VIDEOPIPE_H */

