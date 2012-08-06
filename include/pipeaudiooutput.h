#ifndef _AUDIOOUTPUT_H_
#define _AUDIOOUTPUT_H_
#include <pthread.h>
#include <fifo.h>
#include <audio.h>

class PipeAudioOutput :
	public AudioOutput
	
{
public:
	PipeAudioOutput();
	~PipeAudioOutput();
	virtual int PlayBuffer(WORD *buffer,DWORD size,DWORD frameTime);
	virtual int StartPlaying();
	virtual int StopPlaying();

	int GetSamples(WORD *buffer,DWORD size);
	int Init();
	int End();
private:
	//Mutex
	pthread_mutex_t mutex;

	//Members
	fifo<WORD,2048>	fifoBuffer;
	int 		inited;
	

};

#endif
