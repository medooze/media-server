#ifndef _FLVENCODER_H_
#define _FLVENCODER_H_
#include <pthread.h>
#include "video.h"
#include "audio.h"
#include "codecs.h"
#include "rtmpstream.h"

class FLVEncoder : public RTMPMediaStream
{
public:
	FLVEncoder(DWORD id);
	~FLVEncoder();

	int Init(AudioInput* audioInput,VideoInput *videoInput,const Properties &properties);
	int StartEncoding();
	int StopEncoding();
	int End();
	/* Overrride from RTMPMediaStream*/
	virtual DWORD AddMediaListener(RTMPMediaStream::Listener* listener);
	//Add listenest for media stream
	virtual DWORD AddMediaFrameListener(MediaFrame::Listener* listener);
	virtual DWORD RemoveMediaFrameListener(MediaFrame::Listener* listener);

protected:
	int EncodeAudio();
	int EncodeVideo();

private:
	//Funciones propias
	static void *startEncodingAudio(void *par);
	static void *startEncodingVideo(void *par);
private:
	typedef std::set<MediaFrame::Listener*> MediaFrameListeners;
private:
	AudioCodec::Type	audioCodec;
	AudioInput*		audioInput;
	pthread_t		encodingAudioThread;
	int			encodingAudio;
	Properties		audioProperties;

	VideoCodec::Type	videoCodec;
	VideoInput*		videoInput;
	pthread_t		encodingVideoThread;
	int			encodingVideo;
	Properties		videoProperties;

	RTMPMetaData*	meta;
	RTMPVideoFrame* frameDesc;
	RTMPAudioFrame* aacSpecificConfig;
	int		width;
	int		height;
	int		bitrate;
	int		fps;
	int		intra;

	int		inited;
	bool		sendFPU;
	timeval		first;
	pthread_mutex_t mutex;
	pthread_cond_t	cond;

	MediaFrameListeners mediaListeners;
	
};

#endif
