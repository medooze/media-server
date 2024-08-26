#ifndef _PIPEVIDEOINPUT_H_
#define _PIPEVIDEOINPUT_H_

#include <video.h>
#include <framescaler.h>
#include "pthread.h"

class PipeVideoInput
	: public VideoInput
{
public:
	PipeVideoInput();
	~PipeVideoInput();

	virtual int   StartVideoCapture(int width,int height,int fps);
	virtual VideoBuffer GrabFrame(DWORD timeout);
	virtual void  CancelGrabFrame();
	virtual int   StopVideoCapture();

	int Init();
	int SetFrame(BYTE * buffer, int height, int width);
	int End();

private:

	FrameScaler resizer;
	int videoWidth;
	int videoHeight;
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

#endif
