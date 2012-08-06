#ifndef _PIPVIDEOOUTPUT_H_
#define _PIPVIDEOOUTPUT_H_
#include <pthread.h>
#include <video.h>

class PipeVideoOutput :
	public VideoOutput
{
public:
	PipeVideoOutput(pthread_mutex_t* mutex, pthread_cond_t* cond);
	~PipeVideoOutput();

	virtual int NextFrame(BYTE *pic);
	virtual int SetVideoSize(int width,int height);

	BYTE*	GetFrame();
	int	IsChanged()	{ return isChanged && inited;	};
	int 	GetWidth()	{ return videoWidth;		};
	int 	GetHeight()	{ return videoHeight;		};
	int	Init();
	int	End();
private:
	BYTE*	buffer;
	int	bufferSize;
	int 	videoWidth;
	int	videoHeight;
	int	isChanged;
	int 	inited;

	pthread_mutex_t* videoMixerMutex;
	pthread_cond_t*  videoMixerCond;
};

#endif
