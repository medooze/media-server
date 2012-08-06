#ifndef _PIPEAUDIOINPUT_H_
#define _PIPEAUDIOINPUT_H_
#include <pthread.h>
#include <audio.h>
#include <fifo.h>

class PipeAudioInput : 
	public AudioInput
{
public:
	PipeAudioInput();
	~PipeAudioInput();
	virtual int RecBuffer(WORD *buffer,DWORD size);
	virtual void  CancelRecBuffer();
	virtual int StartRecording();
	virtual int StopRecording();
	int Init();
	int PutSamples(WORD *buffer,DWORD size);
	int End();

private:
	//Los mutex y condiciones
	pthread_mutex_t mutex;
	pthread_cond_t  cond; 

	//Members
	fifo<WORD,2048>	fifoBuffer;
	int		recording;
	int 		inited;
	int		canceled;


};

#endif
