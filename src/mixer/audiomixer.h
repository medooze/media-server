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

	int Init(const Properties &properties);
	int CreateMixer(int id);
	int InitMixer(int id,int sidebarId);
	int SetMixerSidebar(int id,int sidebarId);
	
	int EndMixer(int id);
	int DeleteMixer(int id);
	AudioInput*  GetInput(int id);
	AudioOutput* GetOutput(int id);
	void Process(DWORD numSamples);

	int CreateSidebar();
	int AddSidebarParticipant(int SidebarId,int partId);
	int RemoveSidebarParticipant(int SidebarId,int partId);
	int DeleteSidebar(int SidebarId);
	int End();

	//VAD proxy interface
	virtual DWORD GetVAD(int id);
	
	int SetCalculateVAD(bool vad);

public:
	static int SidebarDefault;
	static int NoSidebar;
	
protected:
	//Mix thread
	int MixAudio();

private:
	//Mixer thread launcher
	static void * startMixingAudio(void *par);

private:

	//Tipos
	class AudioSource
	{
	public:
		AudioSource()
		{
			//Alloc alligned buffer
			buffer = (SWORD*)malloc32(Sidebar::MIXER_BUFFER_SIZE*sizeof(SWORD));
			//No len
			len = 0;
		}
		~AudioSource()
		{
			//Free buffer
			free(buffer);
		}
		SWORD*		buffer;
		DWORD		len;
		PipeAudioInput  *input;
		PipeAudioOutput *output;
		Sidebar*	sidebar;
		DWORD		vad;
	};

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
