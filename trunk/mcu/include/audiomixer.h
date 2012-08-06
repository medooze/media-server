#ifndef _AUDIOMIXER_H_
#define _AUDIOMIXER_H_
#include <pthread.h>
#include <use.h>
#include <audio.h>
#include "pipeaudioinput.h"
#include "pipeaudiooutput.h"
#include <map>

#define MIXER_BUFFER_SIZE 256

class AudioMixer 
{
public:
	AudioMixer();
	~AudioMixer();

	int Init();
	int CreateMixer(int id);
	int InitMixer(int id);
	int EndMixer(int id);
	int DeleteMixer(int id);
	AudioInput*  GetInput(int id);
	AudioOutput* GetOutput(int id);
	void Process(void);
	int End();

protected:
	//Mix thread
	int MixAudio();

private:
	//Mixer thread launcher
	static void * startMixingAudio(void *par);

private:

	//Tipos
	typedef struct 
	{
		PipeAudioInput  *input;
		PipeAudioOutput *output;
		WORD		buffer[MIXER_BUFFER_SIZE];
		DWORD		len;
	} AudioSource;

	typedef std::map<int,AudioSource *> Audios;

	//La lista de audios a mezclar
	Audios lstAudios;

private:
	//Audio mixing buffer
	WORD mixer_buffer[MIXER_BUFFER_SIZE];

	//Threads, mutex y condiciones
	pthread_t 	mixAudioThread;
	int		mixingAudio;
	Use		lstAudiosUse;

};

#endif
