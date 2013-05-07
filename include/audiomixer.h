#ifndef _AUDIOMIXER_H_
#define _AUDIOMIXER_H_
#include <pthread.h>
#include <use.h>
#include <audio.h>
#include "pipeaudioinput.h"
#include "pipeaudiooutput.h"
#include "sidebar.h"
#include <map>

class AudioMixer : public VADProxy
{
public:
	AudioMixer();
	~AudioMixer();

	int Init(bool vad,DWORD rate = 8000);
	int CreateMixer(int id);
	int InitMixer(int id,int sidebarId);
	int SetMixerSidebar(int id,int sidebarId);
	int EndMixer(int id);
	int DeleteMixer(int id);
	AudioInput*  GetInput(int id);
	AudioOutput* GetOutput(int id);
	void Process(void);

	int CreateSidebar();
	int AddSidebarParticipant(int SidebarId,int partId);
	int RemoveSidebarParticipant(int SidebarId,int partId);
	int DeleteSidebar(int SidebarId);
	int End();

	//VAD proxy interface
	virtual DWORD GetVAD(int id);

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
		SWORD		buffer[Sidebar::MIXER_BUFFER_SIZE];
		DWORD		len;
		Sidebar*	sidebar;
		DWORD		vad;
	} AudioSource;

	typedef std::map<int,AudioSource *>	Audios;
	typedef std::map<int,Sidebar *>		Sidebars;

private:
	pthread_t 	mixAudioThread;
	int		mixingAudio;
	Use		lstAudiosUse;
	
	Audios		audios;
	Sidebars	sidebars;
	Sidebar*	defaultSidebar;
	int		numSidebars;
	bool		vad;
	DWORD		rate;

};

#endif
