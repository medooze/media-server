#ifndef _VIDEOMIXER_H_
#define _VIDEOMIXER_H_
#include <pthread.h>
#include <video.h>
#include <use.h>
#include "pipevideoinput.h"
#include "pipevideooutput.h"
#include "mosaic.h"
#include "logo.h"
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

	VideoMixer();
	~VideoMixer();

	int Init(Mosaic::Type comp,int size);
	void SetVADMode(VADMode vadMode);
	void SetVADProxy(VADProxy* proxy);
	int CreateMixer(int id);
	int InitMixer(int id,int mosaicId);
	int SetMixerMosaic(int id,int mosaicId);
	int EndMixer(int id);
	int DeleteMixer(int id);
	VideoInput*  GetInput(int id);
	VideoOutput* GetOutput(int id);
	int SetWatcher(VideoOutput* watcher);
	int SetSlot(int num,int id);
	int SetCompositionType(Mosaic::Type comp,int size);

	int CreateMosaic(Mosaic::Type comp,int size);
	int SetMosaicOverlayImage(int mosaicId,const char* filename);
	int ResetMosaicOverlay(int mosaicId);
	int AddMosaicParticipant(int mosaicId,int partId);
	int RemoveMosaicParticipant(int mosaicId,int partId);
	int SetSlot(int mosaicId,int num,int id);
	int SetCompositionType(int mosaicId,Mosaic::Type comp,int size);
	int DeleteMosaic(int mosaicId);

	int End();

protected:
	int MixVideo();
	int UpdateMosaic(Mosaic* mosaic);
	int GetPosition(int mosaicId,int id);
	
private:
	static void * startMixingVideo(void *par);

private:

	//Tipos
	typedef struct 
	{
		PipeVideoInput  *input;
		PipeVideoOutput *output;
		Mosaic *mosaic;
	} VideoSource;

	typedef std::map<int,VideoSource *> Videos;
	typedef std::map<int,Mosaic *> Mosaics;
private:
	//La lista de videos a mezclar
	Videos lstVideos;
	//Mosaics
	Mosaics mosaics;
	int maxMosaics;

	//Las propiedades del mosaico
	Logo 	logo;
	Mosaic	*defaultMosaic;

	//Threads, mutex y condiciones
	pthread_t 	mixVideoThread;
	pthread_cond_t  mixVideoCond;
	pthread_mutex_t mixVideoMutex;
	int		mixingVideo;
	Use		lstVideosUse;
	VADProxy*	proxy;
	VADMode		vadMode;
};

#endif
