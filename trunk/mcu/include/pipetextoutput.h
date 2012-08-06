#ifndef _PIPETEXTOUTPUT_H_
#define _PIPETEXTOUTPUT_H_
#include <pthread.h>
#include "fifo.h"
#include "text.h"

class PipeTextOutput :
	public TextOutput
	
{
public:
	PipeTextOutput();
	~PipeTextOutput();
	virtual int SendFrame(TextFrame &frame);
	
	int Init();
	int ReadText(wchar_t *buffer,DWORD size);
	int PeekText(wchar_t *buffer,DWORD size);
	int SkipText(DWORD size);
	int Length();
	int End();
private:
	//Mutex
	pthread_mutex_t mutex;

	//Members
	fifo<wchar_t,2048>	fifoBuffer;
	int			inited;
	

};

#endif
