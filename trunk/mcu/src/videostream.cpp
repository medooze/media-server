#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include "videostream.h"
#include "h263/h263codec.h"
#include "h263/mpeg4codec.h"
#include "h264/h264encoder.h"
#include "h264/h264decoder.h"
#include "log.h"
#include "tools.h"
#include "RTPSmoother.h"


/**********************************
* VideoStream
*	Constructor
***********************************/
VideoStream::VideoStream(Listener* listener) : rtp(MediaFrame::Video,listener)
{
	//Inicializamos a cero todo
	sendingVideo=0;
	receivingVideo=0;
	videoInput=NULL;
	videoOutput=NULL;
	videoCodec=VideoCodec::H263_1996;
	videoCaptureMode=0;
	videoGrabWidth=0;
	videoGrabHeight=0;
	videoFPS=0;
	videoBitrate=0;
	videoQuality=0;
	videoFillLevel=0;
	videoIntraPeriod=0;
	sendFPU = false;
	this->listener = listener;
	mediaListener = NULL;
	muted = false;
	//Create objects
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);
}

/*******************************
* ~VideoStream
*	Destructor. Cierra los dispositivos
********************************/
VideoStream::~VideoStream()
{
	//Clean object
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

/**********************************************
* SetVideoCodec
*	Fija el modo de envio de video 
**********************************************/
int VideoStream::SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int quality, int fillLevel,int intraPeriod)
{
	Log("-SetVideoCodec [%s,%dfps,%dkbps,qMax:%d,qMin%d,intra:%d]\n",VideoCodec::GetNameFor(codec),fps,bitrate,quality,fillLevel,intraPeriod);

	//Dependiendo del tipo ponemos las calidades por defecto
	switch(codec)
	{
		case VideoCodec::H263_1996:
			videoQuality=1;
			videoFillLevel=6;
			break;
		case VideoCodec::H263_1998:
			videoQuality=1;
			videoFillLevel=6;
			break;
		case VideoCodec::H264:
			videoQuality=1;
			videoFillLevel=6;
			break;
		default:
			//Return codec
			return Error("Codec not found\n");
	}

	//LO guardamos
	videoCodec=codec;

	//Guardamos el bitrate
	videoBitrate=bitrate;

	//La calidad
	if (quality>0)
		videoQuality = quality;

	//El level
	if (fillLevel>0)
		videoFillLevel = fillLevel;

	//The intra period
	if (intraPeriod>0)
		videoIntraPeriod = intraPeriod;

	//Get width and height
	videoGrabWidth = GetWidth(mode);
	videoGrabHeight = GetHeight(mode);

	//Check size
	if (!videoGrabWidth || !videoGrabHeight)
		//Error
		return Error("Unknown video mode\n");

	//Almacenamos el modo de captura
	videoCaptureMode=mode;

	//Y los fps
	videoFPS=fps;

	return 1;
}

/***************************************
* Init
*	Inicializa los devices 
***************************************/
int VideoStream::Init(VideoInput *input,VideoOutput *output)
{
	Log(">Init video stream\n");

	//Iniciamos el rtp
	if(!rtp.Init())
		return Error("No hemos podido abrir el rtp\n");

	//Init smoother
	smoother.Init(&rtp);

	//Guardamos los objetos
	videoInput  = input;
	videoOutput = output;
	
	//No estamos haciendo nada
	sendingVideo=0;
	receivingVideo=0;

	Log("<Init video stream\n");

	return 1;
}

/**************************************
* startSendingVideo
*	Function helper for thread
**************************************/
void* VideoStream::startSendingVideo(void *par)
{
	Log("SendVideoThread [%d]\n",getpid());

	//OBtenemos el objeto
	VideoStream *conf = (VideoStream *)par;

	//Bloqueamos las se�ales
	blocksignals();

	//Y ejecutamos la funcion
	pthread_exit((void *)conf->SendVideo());
}

/**************************************
* startReceivingVideo
*	Function helper for thread
**************************************/
void* VideoStream::startReceivingVideo(void *par)
{
	Log("RecVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	VideoStream *conf = (VideoStream *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)conf->RecVideo());
}

/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int VideoStream::StartSending(char *sendVideoIp,int sendVideoPort,RTPMap& rtpMap)
{
	Log(">StartSending video [%s,%d]\n",sendVideoIp,sendVideoPort);

	//Si estabamos mandando tenemos que parar
	if (sendingVideo)
		//Y esperamos que salga
		StopSending();

	//Si tenemos video
	if (sendVideoPort==0)
		return Error("No video\n");

	//Iniciamos las sesiones rtp de envio
	if(!rtp.SetRemotePort(sendVideoIp,sendVideoPort))
		return Error("Error abriendo puerto rtp\n");

	//Set sending map
	rtp.SetSendingRTPMap(rtpMap);
	
	//Set video codec
	if(!rtp.SetSendingCodec(videoCodec))
		//Error
		return Error("%s video codec not supported by peer\n",VideoCodec::GetNameFor(videoCodec));

	//Estamos mandando
	sendingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&sendVideoThread,startSendingVideo,this,0);

	//LOgeamos
	Log("<StartSending video [%d]\n",sendingVideo);

	return 1;
}

/***************************************
* StartReceiving
*	Abre los sockets y empieza la recetpcion
****************************************/
int VideoStream::StartReceiving(RTPMap& rtpMap)
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingVideo)
		StopReceiving();	

	//Iniciamos las sesiones rtp de recepcion
	int recVideoPort= rtp.GetLocalPort();

	//Set receving map
	rtp.SetReceivingRTPMap(rtpMap);

	//Estamos recibiendo
	receivingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&recVideoThread,startReceivingVideo,this,0);

	//Logeamos
	Log("-StartReceiving Video [%d]\n",recVideoPort);

	return recVideoPort;
}

/***************************************
* End
*	Termina la conferencia activa
***************************************/
int VideoStream::End()
{
	Log(">End\n");

	//Terminamos la recepcion
	if (sendingVideo)
		StopSending();

	//Y el envio
	if(receivingVideo)
		StopReceiving();

	//Close smoother
	smoother.End();

	//Cerramos la session de rtp
	rtp.End();

	Log("<End\n");

	return 1;
}

/***************************************
* StopSending
*	Termina el envio
***************************************/
int VideoStream::StopSending()
{
	Log(">StopSending [%d]\n",sendingVideo);

	//Esperamos a que se cierren las threads de envio
	if (sendingVideo)
	{
		//Paramos el envio
		sendingVideo=0;

		//Cencel video grab
		videoInput->CancelGrabFrame();

		//Cancel sending
		pthread_cond_signal(&cond);

		//Y esperamos
		pthread_join(sendVideoThread,NULL);
	}

	Log("<StopSending\n");

	return 1;
}

/***************************************
* StopReceiving
*	Termina la recepcion
***************************************/
int VideoStream::StopReceiving()
{
	Log(">StopReceiving\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingVideo)
	{
		//Dejamos de recivir
		receivingVideo=0;

		//Esperamos
		pthread_join(recVideoThread,NULL);
	}

	Log("<StopReceiving\n");

	return 1;
}

/*******************************************
* SendVideo
*	Capturamos el video y lo mandamos
*******************************************/
int VideoStream::SendVideo()
{
	timeval first;
	timeval prev;
	
	Log(">SendVideo [width:%d,size:%d,bitrate:%d,fps:%d,intra:%d]\n",videoGrabWidth,videoGrabHeight,videoBitrate,videoFPS,videoIntraPeriod);

	//Creamos el encoder
	VideoEncoder* videoEncoder = VideoCodecFactory::CreateEncoder(videoCodec);

	//Comprobamos que se haya creado correctamente
	if (videoEncoder == NULL)
		//error
		return Error("Can't create video encoder\n");

	//Comrpobamos que tengamos video de entrada
	if (videoInput == NULL)
		return Error("No video input");

	//Iniciamos el tama�o del video
	if (!videoInput->StartVideoCapture(videoGrabWidth,videoGrabHeight,videoFPS))
		return Error("Couldn't set video capture\n");

	//Iniciamos el birate y el framerate
	videoEncoder->SetFrameRate(videoFPS,videoBitrate,videoIntraPeriod);

	//No wait for first
	DWORD frameTime = 0;

	//Iniciamos el tamama�o del encoder
 	videoEncoder->SetSize(videoGrabWidth,videoGrabHeight);

	//The time of the first one
	gettimeofday(&first,NULL);

	//The time of the previos one
	gettimeofday(&prev,NULL);
	
	//Started
	Log("-Sending video\n");

	//Mientras tengamos que capturar
	while(sendingVideo)
	{
		//Nos quedamos con el puntero antes de que lo cambien
		BYTE *pic = videoInput->GrabFrame();

		//Check picture
		if (!pic)
			//Exit
			continue;

		//Check if we need to send intra
		if (sendFPU)
		{
			//Set it
			videoEncoder->FastPictureUpdate();
			//Do not send anymore
			sendFPU = false;
		}
		//Procesamos el frame
		VideoFrame *videoFrame = videoEncoder->EncodeFrame(pic,videoInput->GetBufferSize());

		//If was failed
		if (!videoFrame)
			//Next
			continue;

		//Check
		if (frameTime)
		{
			timespec ts;
			//Lock
			pthread_mutex_lock(&mutex);
			//Calculate timeout
			calcAbsTimeout(&ts,&prev,frameTime);
			//Wait next or stopped
			int canceled  = !pthread_cond_timedwait(&cond,&mutex,&ts);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Check if we have been canceled
			if (canceled)
				//Exit
				break;
		}
		
		//Set next one
		frameTime = 1000/videoFPS;

		//Set frame timestamp
		videoFrame->SetTimestamp(getDifTime(&first)/1000);

		//Check if we have mediaListener
		if (mediaListener)
			//Call it
			mediaListener->onMediaFrame(*videoFrame);

		//Set sending time of previous frame
		getUpdDifTime(&prev);
		
		//Send it smoothly
		smoother.SendFrame(videoFrame,frameTime);
	}

	Log("-SendVideo out of loop\n");

	//Terminamos de capturar
	videoInput->StopVideoCapture();

	//Borramos el encoder
	delete videoEncoder;

	//Salimos
	Log("<SendVideo [%d]\n",sendingVideo);

	return 0;
}

/****************************************
* RecVideo
*	Obtiene los packetes y los muestra
*****************************************/
int VideoStream::RecVideo()
{
	VideoDecoder*	videoDecoder = NULL;
	VideoCodec::Type type;
	int 		recFrames=0;
	int 		recBytes=0;
	timeval 	before;
	timeval		lastFPURequest;
	float 		dif;
	int 		width=0;
	int 		height=0;
	
	Log(">RecVideo\n");
	
	//Inicializamos el tiempo
	gettimeofday(&before,NULL);

	//Not sent FPU yet
	setZeroTime(&lastFPURequest);

	//Mientras tengamos que capturar
	while(receivingVideo)
	{
		//Obtenemos el paquete
		RTPPacket* packet = rtp.GetPacket();

		//Check
		if (!packet)
			//Next
			continue;

		//Get type
		type = (VideoCodec::Type)packet->GetCodec();
		
		//Comprobamos el tipo
		if ((videoDecoder==NULL) || (type!=videoDecoder->type))
		{
			//Si habia uno nos lo cargamos
			if (videoDecoder!=NULL)
				delete videoDecoder;

			//Creamos uno dependiendo del tipo
			videoDecoder = VideoCodecFactory::CreateDecoder(type);

			//Si es nulo
			if (videoDecoder==NULL)
			{
				Error("Error creando nuevo decodificador de video [%d]\n",type);
				continue;
			}
		}

		//Si hemos perdido un paquete
		if(rtp.GetLost())
		{
			//Debug
			Log("Lost packet\n");
			
			//Check if we got listener and more than two seconds have elapsed from last request
			if (listener && getDifTime(&lastFPURequest)>2000000)
			{
				//Debug
				Log("-Requesting FPU\n");
				//Request it
				listener->onRequestFPU();
				//Update time
				getUpdDifTime(&lastFPURequest);
			}
		}

		//Aumentamos el numero de bytes recividos
		recBytes += packet->GetSize();

		//Lo decodificamos
		if(!videoDecoder->DecodePacket(packet->GetMediaData(),packet->GetMediaLength(),0,packet->GetMark()))
		{
			//Check if we got listener and more than two seconds have elapsed from last request
			if (listener && getDifTime(&lastFPURequest)>2000000)
			{
				//Debug
				Log("-Requesting FPU\n");
				//Request it
				listener->onRequestFPU();
				//Update time
				getUpdDifTime(&lastFPURequest);
			}
			//Next frame
			continue;
		}
		
		//Si es el ultimo
		if(packet->GetMark())
		{
			//Comprobamos el tama�o
			if (width!=videoDecoder->GetWidth() || height!=videoDecoder->GetHeight())
			{
				//A cambiado o es la primera vez
				width = videoDecoder->GetWidth();
				height= videoDecoder->GetHeight();

				Log("-Changing video size %dx%d\n",width,height);

				//POnemos el modo de pantalla
				if (!videoOutput->SetVideoSize(width,height))
				{
					Error("Can't update video size\n");
					break;;
				}
			}

			//Get picture
			BYTE *frame = videoDecoder->GetFrame();
			
			//Check size
			if (width && height && frame && !muted)
				//Lo sacamos
				videoOutput->NextFrame(frame);
			
			//Aumentamos el numero de frames y de bytes
			recFrames++;
			
			//Cada 20 frames sacamos las estadisticas
			if ((videoFPS && (recFrames==videoFPS*2)) || recFrames==60)
			{
				//Y la diferencia
				dif=getUpdDifTime(&before);
	
				//Y reseteamos
				recFrames=0;
				recBytes=0;
			}
		}
	}

	//Borramos el encoder
	delete videoDecoder;

	Log("<RecVideo\n");

	//Salimos
	pthread_exit(0);
}

int VideoStream::SetMediaListener(MediaFrame::Listener *listener)
{
	//Set it
	this->mediaListener = listener;
}

int VideoStream::SendFPU()
{
	//Next shall be an intra
	sendFPU = true;
	
	return 1;
}

MediaStatistics VideoStream::GetStatistics()
{
	MediaStatistics stats;

	//Fill stats
	stats.isReceiving	= IsReceiving();
	stats.isSending		= IsSending();
	stats.lostRecvPackets   = rtp.GetLostRecvPackets();
	stats.numRecvPackets	= rtp.GetNumRecvPackets();
	stats.numSendPackets	= rtp.GetNumSendPackets();
	stats.totalRecvBytes	= rtp.GetTotalRecvBytes();
	stats.totalSendBytes	= rtp.GetTotalSendBytes();

	//Return it
	return stats;
}

int VideoStream::SetMute(bool isMuted)
{
	//Set it
	muted = isMuted;
	//Exit
	return 1;
}
