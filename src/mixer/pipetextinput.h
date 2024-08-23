#ifndef _PIPETEXTINPUT_H_
#define _PIPETEXTINPUT_H_
#include <pthread.h>
#include "text.h"
#include <list>

class PipeTextInput :
	public TextInput
{
public:
	PipeTextInput();
	~PipeTextInput();
	virtual TextFrame* GetFrame(DWORD timeout);
	virtual void Cancel();
	int Init();
	int WriteText(const wchar_t *data,DWORD size);
	int WriteText(const std::wstring &str);
	int End();

private:
	//Los mutex y condiciones
	pthread_mutex_t mutex;
	pthread_cond_t  cond; 

	//Members
	std::list<TextFrame*> frames;
	int 		inited;
	timeval		first;
};

#endif
