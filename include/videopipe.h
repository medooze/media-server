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
#include "framescaler.h"

class VideoPipe :
	public VideoOutput,
	public VideoInput
{
public:
	VideoPipe();
	~VideoPipe();
	int Init();
	/** VideoInput */
	virtual int   StartVideoCapture(int width,int height,int fps);
	virtual BYTE* GrabFrame(DWORD timeout);
	virtual void  CancelGrabFrame();
	virtual DWORD GetBufferSize();
	virtual int   StopVideoCapture();
	/** VideoOutput */
	virtual int NextFrame(BYTE *pic);
	virtual void ClearFrame();
	virtual int SetVideoSize(int width,int height);
	int End();
private:

	FrameScaler resizer;
	int videoWidth;
	int videoHeight;
	int inputWidth;
	int inputHeight;
	int videoSize;
	int videoFPS;
	int imgPos;
	int imgNew;
	int inited;
	int capturing;
	BYTE *imgBuffer[2];
	BYTE *grabPic;

	pthread_mutex_t newPicMutex;
	pthread_cond_t  newPicCond;
};

#endif	/* VIDEOPIPE_H */

