/* 
 * File:   videopipe.h
 * Author: Sergio
 *
 * Created on 19 de marzo de 2013, 16:08
 */

#ifndef VIDEOPIPE_H
#define	VIDEOPIPE_H

#include <pthread.h>
#include "video.h"
#include "VideoBufferScaler.h"

class VideoPipe :
	public VideoOutput,
	public VideoInput
{
public:
	VideoPipe();
	virtual ~VideoPipe();
	int Init(float scaleResolutionDownBy = 0.0f);
	/** VideoInput */
	virtual int   StartVideoCapture(uint32_t width, uint32_t height, uint32_t fps);
	virtual VideoBuffer::const_shared GrabFrame(uint32_t timeout);
	virtual void  CancelGrabFrame();
	virtual int   StopVideoCapture();
	/** VideoOutput */
	virtual int NextFrame(const VideoBuffer::const_shared& videoBuffer);
	virtual void ClearFrame();
	int End();
private:
	uint32_t videoWidth = 0;
	uint32_t videoHeight = 0;
	int videoFPS = 0;
	int imgPos = 0;
	int imgNew = false;
	float scaleResolutionDownBy = 0.0f;
	int inited = false;
	int capturing = false;
	VideoBuffer::const_shared imgBuffer[2];

	pthread_mutex_t newPicMutex;
	pthread_cond_t  newPicCond;

	VideoBufferPool	videoBufferPool;
	VideoBufferScaler scaler;

};

#endif	/* VIDEOPIPE_H */

