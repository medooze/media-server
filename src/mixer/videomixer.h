#ifndef _VIDEOMIXER_H_
#define _VIDEOMIXER_H_
#include <pthread.h>
#include <video.h>
#include <use.h>
#include "pipevideoinput.h"
#include "pipevideooutput.h"
#include "mosaic.h"
#include "logo.h"
#include "EventSource.h"
#include <list>
#include <map>

class VideoMixer 
{
public:
	enum VADMode
	{
		NoVAD	 = 0,
		BasicVAD = 1,
		FullVAD  = 2
	};
public:
	// Los valores indican el n�mero de mosaicos por composicion

	VideoMixer(const std::wstring &tag);
	~VideoMixer();

	int Init(const Properties &properties);
	void SetKeepAspectRatio(bool keepAspectRatio);
	void SetDiplayNames(bool displayNames);
	void SetVADMode(VADMode vadMode);
	void SetVADProxy(VADProxy* proxy);
	int CreateMixer(int id,const std::wstring &name);
	int InitMixer(int id,int mosaicId);
	int SetMixerMosaic(int id,int mosaicId);
	int SetMixerName(int id,const std::wstring &name);
	int EndMixer(int id);
	int DeleteMixer(int id);
	VideoInput*  GetInput(int id);
	VideoOutput* GetOutput(int id);
	int SetSlot(int num,int id);

	int CreateMosaic(Mosaic::Type comp,int size);
	int SetMosaicOverlayImage(int mosaicId,const char* filename);
	int ResetMosaicOverlay(int mosaicId);
	int AddMosaicParticipant(int mosaicId,int partId);
	int RemoveMosaicParticipant(int mosaicId,int partId);
	int GetMosaicPositions(int mosaicId,std::list<int> &positions);
	int SetSlot(int mosaicId,int num,int id);
	int ResetSlots(int mosaicId);
	int SetCompositionType(int mosaicId,Mosaic::Type comp,int size);
	int SetMosaicPadding(int mosaicId, int paddingTop = 0, int paddingRight = 0, int paddingBottom = 0, int paddingLeft = 0);
	int SetMosaicOverlayText(int mosaicId);
	int RenderMosaicOverlayText(int mosaicId,const std::wstring& text,DWORD x,DWORD y,DWORD width,DWORD height, const Properties &properties);
	int RenderMosaicOverlayText(int mosaicId,const std::string& utf8,DWORD x,DWORD y,DWORD width,DWORD height, const Properties &properties);
	int DeleteMosaic(int mosaicId);

	void Process(bool forceUpdate, QWORD now);
	int End();
	
public:
	static int MosaicDefault;
	static int NoMosaic;	
public:
	static void SetVADDefaultChangePeriod(DWORD ms);

protected:
	int MixVideo();
	int DumpMosaic(DWORD id,Mosaic* mosaic);
	int GetPosition(int mosaicId,int id);
	
private:
	static void * startMixingVideo(void *par);

private:

	struct VideoSource
	{
		PipeVideoInput  *input;
		PipeVideoOutput *output;
		Mosaic *mosaic;
		std::wstring name;
		bool refresh;
		
		VideoSource(const std::wstring &name)
		{
			//Store name
			this->name = name;
			//NULL
			input = NULL;
			output = NULL;
			mosaic = NULL;
			refresh = true;
			
		}
	};

	typedef std::map<int,VideoSource *> Videos;
	typedef std::map<int,Mosaic *> Mosaics;
private:
	static DWORD vadDefaultChangePeriod;
private:
	EvenSource	eventSource;
	std::wstring tag;
	//La lista de videos a mezclar
	Videos lstVideos;
	//Mosaics
	Mosaics mosaics;
	int maxMosaics = MosaicDefault;

	//Las propiedades del mosaico
	Logo 	logo;
	Mosaic	*defaultMosaic			 = nullptr;

	//Threads, mutex y condiciones
	pthread_t 	mixVideoThread;
	pthread_cond_t  mixVideoCond;
	pthread_mutex_t mixVideoMutex;
	int		mixingVideo		= 0;
	QWORD		ini			= 0;
	Use		lstVideosUse;
	VADProxy*	proxy			= nullptr;
	VADMode		vadMode			= VADMode::NoVAD;
	bool		keepAspectRatio		= true;
	bool		displayNames		= false;
	uint32_t	speakingThreshold	= 0;
	DWORD		version = 0;
	Properties	overlay;
	Properties	overlaySpeaking;
};

#endif
