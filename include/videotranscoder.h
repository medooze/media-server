#ifndef _VIDEOTRANSCODER_H_
#define _VIDEOTRANSCODER_H_

#include <pthread.h>
#include "config.h"
#include "codecs.h"
#include "rtpsession.h"
#include "video.h"

class VideoTranscoder
{
public:
	VideoStream();
	~VideoStream();

	//Opciones de codec
	int SetVideoCodec(int codec,int mode,int fps,int bitrate,int quality=0, int fillLevel=0);
	int IsCodecSupported(int codec);
	
	int Init(VideoInput *input);
	int Start();
	int Add();
	int Stop();
	int End();

	int IsStarted()	  { return started;  }

protected:
	int Video();

private:
	static void* start(void *par);

	//Funciones propias
	VideoEncoder* CreateVideoEncoder(int type);
	VideoDecoder* CreateVideoDecoder(int type);

	//Video origin
	VideoInput     	*videoInput;

	VideoOutput 	*videoOutput;
	RTPSession      rtp;

	//Parametros del video
	int		videoType;		//Codec de envio
	int		videoCaptureMode;	//Modo de captura de video actual
	int 		videoGrabWidth;		//Ancho de la captura
	int 		videoGrabHeight;	//Alto de la captur
	int 		videoFPS;
	int 		videoBitrate;
	int		videoQuality;
	int		videoFillLevel;

	//Las threads
	pthread_t 	sendVideoThread;
	pthread_t 	recVideoThread;

	//Controlamos si estamos mandando o no
	volatile short	sendingVideo;	
	volatile short 	receivingVideo;
	volatile short	inited;

};

#endif
