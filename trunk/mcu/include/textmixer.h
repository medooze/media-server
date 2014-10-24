#ifndef _TEXTMIXER_H_
#define _TEXTMIXER_H_
#include <pthread.h>
#include "use.h"
#include "wait.h"
#include "text.h"
#include "textmixerworker.h"
#include "pipetextinput.h"
#include "pipetextoutput.h"
#include "pipeaudioinput.h"
#include "textmixerworker.h"
#include <map>
#include <set>
using namespace std;

class TextMixer
{
public:
	TextMixer();
	~TextMixer();

	//Global members
	static int GlobalInit();
	static int GlobalEnd();

	int Init();
	int CreateMixer(int id,const std::wstring &name);
	int InitMixer(int id);
	int EndMixer(int id);
	int DeleteMixer(int id);
	TextInput*  GetInput(int id);
	TextOutput* GetOutput(int id);
	int CreatePrivate(int id,int to,std::wstring &name);
	int InitPrivate(int id);
	int EndPrivate(int id);
	int DeletePrivate(int id);
	TextOutput* GetPrivateOutput(int id);
	int End();

protected:
	//Mix thread
	int MixText();

private:
	//Mixer thread launcher
	static void * startMixingText(void *par);

	struct TextSource
	{
		DWORD id;
		std::wstring	name;
		PipeTextInput	*input;
		PipeTextOutput	*output;
		TextMixerWorker *worker;
	};

	struct TextPrivate
	{
		DWORD id;
		DWORD to;
		std::wstring	name;
		PipeTextOutput	*output;
	};

	typedef std::map<DWORD,TextSource*> TextSources;
	typedef std::map<DWORD,TextPrivate*> TextPrivates;
	typedef std::set<TextMixerWorker*> TextWorkers;
	
private:
	//All the mixer participants
	TextSources	sources;
	TextWorkers	workers;
	TextPrivates	privates;
	//Threads, mutex y condiciones
	pthread_t 	mixTextThread;
	pthread_mutex_t	mixTextMutex;
	int		mixingText;
	Use		lstTextsUse;
	Wait		cancel;
};

#endif
