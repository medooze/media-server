#ifndef _PIPEAUDIOINPUT_H_
#define _PIPEAUDIOINPUT_H_
#include <pthread.h>
#include <audio.h>
#include <fifo.h>
#include "audiotransrater.h"


class PipeAudioInput : 
	public AudioInput
{
public:
	PipeAudioInput();
	~PipeAudioInput();
	virtual int RecBuffer(SWORD *buffer,DWORD size);
	virtual int ClearBuffer();
	virtual void CancelRecBuffer();
	virtual int StartRecording(DWORD rate);
	virtual int StopRecording();

	virtual DWORD GetNativeRate()		{ return nativeRate;	}
	virtual DWORD GetRecordingRate()	{ return recordRate;	}
	virtual DWORD GetNumChannels()		{ return 1;		}
	
	int Init(DWORD rate);
	int PutSamples(SWORD *buffer,DWORD size);
	int End();

private:
	//Los mutex y condiciones
	pthread_mutex_t mutex;
	pthread_cond_t  cond; 

	//Members
	fifo<SWORD,4096>	fifoBuffer;
	int		recording;
	int 		inited;
	int		canceled;
	
	
	AudioTransrater		transrater;
	DWORD			recordRate;
	DWORD			nativeRate;
};

#endif
