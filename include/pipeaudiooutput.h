#ifndef _AUDIOOUTPUT_H_
#define _AUDIOOUTPUT_H_
#include <pthread.h>
#include <fifo.h>
#include <audio.h>
#include "vad.h"

class PipeAudioOutput :
	public AudioOutput
	
{
public:
	PipeAudioOutput(bool calcVAD);
	~PipeAudioOutput();
	virtual int PlayBuffer(SWORD *buffer,DWORD size,DWORD frameTime);
	virtual int StartPlaying();
	virtual int StopPlaying();

	int GetSamples(SWORD *buffer,DWORD size);
	DWORD GetVAD(DWORD numSamples);
	int Init();
	int End();
private:
	//Mutex
	pthread_mutex_t mutex;

	//Members
	fifo<SWORD,2048>	fifoBuffer;
	int			inited;
	VAD			vad;
	DWORD			acu;
	bool			calcVAD;
};

#endif
