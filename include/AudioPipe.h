#ifndef AUDIOPIPE_H
#define AUDIOPIPE_H
#include <pthread.h>
#include <audio.h>
#include <fifo.h>
#include "audiotransrater.h"


class AudioPipe : 
	public AudioInput,
	public AudioOutput
{
public:
	AudioPipe(DWORD rate);
	virtual ~AudioPipe();
	
	//Audio input
	virtual int RecBuffer(SWORD *buffer,DWORD size);
	virtual int ClearBuffer();
	virtual void CancelRecBuffer();
	virtual int StartRecording(DWORD rate);
	virtual int StopRecording();

	//Audio output
	virtual int PlayBuffer(SWORD *buffer,DWORD size,DWORD frameTime, BYTE vadLevel = -1);
	virtual int StartPlaying(DWORD samplerate, DWORD numChannels);
	virtual int StopPlaying();

	virtual DWORD GetNativeRate()		{ return nativeRate;	}
	virtual DWORD GetPlayingRate()		{ return playRate;	}
	virtual DWORD GetRecordingRate()	{ return recordRate;	}
	virtual DWORD GetNumChannels()		{ return numChannels;	}
	
private:
	//Los mutex y condiciones
	pthread_mutex_t mutex;
	pthread_cond_t  cond; 

	//Members
	fifo<SWORD,48000*4>	fifoBuffer;
	bool			recording = false;
	bool			playing = false;
	bool			inited = false;
	bool			canceled = false;
	
	AudioTransrater		transrater;
	DWORD			playRate = 0;
	DWORD			recordRate = 0;
	DWORD			nativeRate = 0;
	DWORD			numChannels = 1;
	
	DWORD			cache = 0;
};

#endif /* AUDIOPIPE_H */

