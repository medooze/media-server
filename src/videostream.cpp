#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <math.h>
#include "videostream.h"
#include "h263/h263codec.h"
#include "h263/mpeg4codec.h"
#include "h264/h264encoder.h"
#include "h264/h264decoder.h"
#include "log.h"
#include "tools.h"
#include "acumulator.h"
#include "RTPSmoother.h"
#include "mp4recorder.h"
#include "VideoCodecFactory.h"

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
	videoIntraPeriod=0;
	videoBitrateLimit=0;
	videoBitrateLimitCount=0;
	minFPUPeriod = 500;
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
int VideoStream::SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties& properties)
{
	Log("-SetVideoCodec [%s,%dfps,%dkbps,intra:%d]\n",VideoCodec::GetNameFor(codec),fps,bitrate,intraPeriod);

	//Fix: Should not be here
	if (properties.HasProperty("rateEstimator.maxRate"))
		rtp.SetTemporalMaxLimit(properties.GetProperty("rateEstimator.maxRate",0));
	//Fix: Should not be here
	if (properties.HasProperty("rateEstimator.minRate"))
		//Set it
		rtp.SetTemporalMinLimit(properties.GetProperty("rateEstimator.minRate",0));
	
	//LO guardamos
	videoCodec=codec;

	//Guardamos el bitrate
	videoBitrate=bitrate;

	//Store properties
	videoProperties = properties;
	
	//Get min FPU period
	minFPUPeriod = videoProperties.GetProperty("video.minFPUPeriod",500);

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

int VideoStream::SetTemporalBitrateLimit(int estimation)
{
	//Set bitrate limit
	videoBitrateLimit = estimation/1000;
	//Set limit of bitrate to 1 second;
	videoBitrateLimitCount = videoFPS;
	//Exit
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
	
	//The time of init
	gettimeofday(&ini,NULL);

	Log("<Init video stream\n");

	return 1;
}

int VideoStream::SetLocalCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetLocalCryptoSDES(suite,key64);
}

int VideoStream::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetRemoteCryptoSDES(suite,key64);
}

int VideoStream::SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint)
{
	return rtp.SetRemoteCryptoDTLS(setup,hash,fingerprint);
}

int VideoStream::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetLocalSTUNCredentials(username,pwd);
}

int VideoStream::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetRemoteSTUNCredentials(username,pwd);
}
int VideoStream::SetRTPProperties(const Properties& properties)
{
	return rtp.SetProperties(properties);
}
/**************************************
* startSendingVideo
*	Function helper for thread
**************************************/
void* VideoStream::startSendingVideo(void *par)
{
	Log("SendVideoThread [%p]\n",pthread_self());

	//OBtenemos el objeto
	VideoStream *conf = (VideoStream *)par;

	//Bloqueamos las se�ales
	blocksignals();

	//Y ejecutamos la funcion
	conf->SendVideo();
	//Exit
	return NULL;
}

/**************************************
* startReceivingVideo
*	Function helper for thread
**************************************/
void* VideoStream::startReceivingVideo(void *par)
{
	Log("RecVideoThread [%p]\n",pthread_self());

	//Obtenemos el objeto
	VideoStream *conf = (VideoStream *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	conf->RecVideo();
	//Exit
	return NULL;
}

/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int VideoStream::StartSending(char *sendVideoIp,int sendVideoPort,const RTPMap& rtpMap,const RTPMap& aptMap)
{
	Log(">StartSendingVideo [%s,%d]\n",sendVideoIp,sendVideoPort);

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
	rtp.SetSendingRTPMap(rtpMap,aptMap);
	
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
int VideoStream::StartReceiving(const RTPMap& rtpMap,const RTPMap& aptMap)
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingVideo)
		StopReceiving();
	
	//Iniciamos las sesiones rtp de recepcion
	int recVideoPort= rtp.GetLocalPort();

	//Set receving map
	rtp.SetReceivingRTPMap(rtpMap,aptMap);

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

		//Check we have video
		if (videoInput)
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

		//Cancel rtp
		rtp.CancelGetPacket();
		
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
	timeval prev;
	timeval lastFPU;
	
	DWORD num = 0;
	QWORD overslept = 0;

	Acumulator bitrateAcu(1000);
	Acumulator fpsAcu(1000);
	
	Log(">SendVideo [width:%d,size:%d,bitrate:%d,fps:%d,intra:%d]\n",videoGrabWidth,videoGrabHeight,videoBitrate,videoFPS,videoIntraPeriod);

	//Creamos el encoder
	VideoEncoder* videoEncoder = VideoCodecFactory::CreateEncoder(videoCodec,videoProperties);

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

	//Start at 80%
	int current = videoBitrate*0.8;

	//Send at higher bitrate first frame, but skip frames after that so sending bitrate is kept
	videoEncoder->SetFrameRate(videoFPS,current*5,videoIntraPeriod);

	//No wait for first
	QWORD frameTime = 0;

	//Iniciamos el tamama�o del encoder
 	videoEncoder->SetSize(videoGrabWidth,videoGrabHeight);

	//The time of the previos one
	gettimeofday(&prev,NULL);

	//Fist FPU
	gettimeofday(&lastFPU,NULL);
	
	//Started
	Log("-Sending video\n");

	//Mientras tengamos que capturar
	while(sendingVideo)
	{
		//Nos quedamos con el puntero antes de que lo cambien
		auto pic = videoInput->GrabFrame(frameTime/1000);

		//Check picture
		if (!pic.buffer)
			//Exit
			continue;

		//Check if we need to send intra
		if (sendFPU)
		{
			//Do not send anymore
			sendFPU = false;
			//Do not send if we just send one (100ms)
			if (getDifTime(&lastFPU)/1000>minFPUPeriod)
			{
				//Send at higher bitrate first frame, but skip frames after that so sending bitrate is kept
				videoEncoder->SetFrameRate(videoFPS,current*5,videoIntraPeriod);
				//Reste frametime so it is calcualted afterwards
				frameTime = 0;
				//Set it
				videoEncoder->FastPictureUpdate();
				//Update last FPU
				getUpdDifTime(&lastFPU);
			}
		}

		//Calculate target bitrate
		int target = current;

		//Check temporal limits for estimations
		if (bitrateAcu.IsInWindow())
		{
			//Get real sent bitrate during last second and convert to kbits 
			DWORD instant = bitrateAcu.GetInstantAvg()/1000;
			//If we are in quarentine
			if (videoBitrateLimitCount)
				//Limit sending bitrate
				target = videoBitrateLimit;
			//Check if sending below limits
			else if (instant<videoBitrate)
				//Increase a 8% each second or fps kbps
				target += (DWORD)(target*0.08/videoFPS)+1;
		}

		//Check target bitrate agains max conf bitrate
		if (target>videoBitrate*1.2)
			//Set limit to max bitrate allowing a 20% overflow so instant bitrate can get closer to target
			target = videoBitrate*1.2;

		//Check limits counter
		if (videoBitrateLimitCount>0)
			//One frame less of limit
			videoBitrateLimitCount--;

		//Check if we have a new bitrate
		if (target && target!=current)
		{
			//Reset bitrate
			videoEncoder->SetFrameRate(videoFPS,target,videoIntraPeriod);
			//Upate current
			current = target;
		}
		
		//Procesamos el frame
		VideoFrame *videoFrame = videoEncoder->EncodeFrame(pic.buffer,pic.GetBufferSize());

		//If was failed
		if (!videoFrame)
			//Next
			continue;
		
		//Increase frame counter
		fpsAcu.Update(getTime()/1000,1);
		
		//Check
		if (frameTime)
		{
			timespec ts;
			//Lock
			pthread_mutex_lock(&mutex);
			//Calculate slept time
			QWORD sleep = frameTime;
			//Remove extra sleep from prev
			if (overslept<sleep)
				//Remove it
				sleep -= overslept;
			else
				//Do not overflow
				sleep = 1;

			//Calculate timeout
			calcAbsTimeoutNS(&ts,&prev,sleep);
			//Wait next or stopped
			int canceled  = !pthread_cond_timedwait(&cond,&mutex,&ts);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Check if we have been canceled
			if (canceled)
				//Exit
				break;
			//Get differencence
			QWORD diff = getDifTime(&prev);
			//If it is biffer
			if (diff>frameTime)
				//Get what we have slept more
				overslept = diff-frameTime;
			else
				//No oversletp (shoulddn't be possible)
				overslept = 0;
		}

		//Increase frame counter
		fpsAcu.Update(getTime()/1000,1);
		
		//If first
		if (!frameTime)
		{
			//Set frame time, slower
			frameTime = 5*1000000/videoFPS;
			//Restore bitrate
			videoEncoder->SetFrameRate(videoFPS,current,videoIntraPeriod);
		} else {
			//Set frame time
			frameTime = 1000000/videoFPS;
		}
		
		//Add frame size in bits to bitrate calculator
		bitrateAcu.Update(getDifTime(&ini)/1000,videoFrame->GetLength()*8);

		//Set clock rate
		videoFrame->SetClockRate(1000);
		
		//Set frame timestamp
		videoFrame->SetTimestamp(getDifTime(&ini)/1000);

		//Check if we have mediaListener
		if (mediaListener)
			//Call it
			mediaListener->onMediaFrame(*videoFrame);

		//Set sending time of previous frame
		getUpdDifTime(&prev);

		//Calculate sending times based on bitrate
		DWORD sendingTime = videoFrame->GetLength()*8/current;

		//Adjust to maximum time
		if (sendingTime>frameTime/1000)
			//Cap it
			sendingTime = frameTime/1000;

		//If it was a I frame
		if (videoFrame->IsIntra())
			//Clean rtp rtx buffer
			rtp.FlushRTXPackets();

		//Send it smoothly
		smoother.SendFrame(videoFrame,sendingTime);

		//Dump statistics
		if (num && ((num%videoFPS*10)==0))
		{
			Debug("-Send bitrate target=%d current=%d avg=%llf rate=[%llf,%llf] fps=[%llf,%llf] limit=%d\n",target,current,bitrateAcu.GetInstantAvg()/1000,bitrateAcu.GetMinAvg()/1000,bitrateAcu.GetMaxAvg()/1000,fpsAcu.GetMinAvg(),fpsAcu.GetMaxAvg(),videoBitrateLimit);
			bitrateAcu.ResetMinMax();
			fpsAcu.ResetMinMax();
		}
		num++;
	}

	Log("-SendVideo out of loop\n");

	//Terminamos de capturar
	videoInput->StopVideoCapture();

	//Check
	if (videoEncoder)
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
	timeval 	now;
	timeval		lastFPURequest;
	DWORD		lostCount=0;
	QWORD		frameTime = (QWORD)-1;
	DWORD		lastSeq = RTPPacket::MaxExtSeqNum;
	bool		waitIntra = false;
	
	Acumulator deliverTimeAcu(1000);
	Acumulator decodeTimeAcu(1000);
	Acumulator waitTimeAcu(1000);
	Acumulator fpsAcu(1000);
	
	Log(">RecVideo\n");
	
	//Get now
	getUpdDifTime(&now);

	//Not sent FPU yet
	setZeroTime(&lastFPURequest);
	
	//Mientras tengamos que capturar
	while(receivingVideo)
	{
		//Update before waiting
		getUpdDifTime(&now);
		
		//Get RTP packet
		auto packet = rtp.GetPacket();
		
		//Get diff
		auto diff = getUpdDifTime(&now)/1000;
		
		//Update waited time
		waitTimeAcu.Update(getTime(now)/1000,diff);

		//Check
		if (!packet)
			//Next
			continue;
		
		//Get extended sequence number and timestamp
		DWORD seq = packet->GetExtSeqNum();
		QWORD ts = packet->GetExtTimestamp();

		//Get packet data
		const BYTE* buffer = packet->GetMediaData();
		DWORD size = packet->GetMediaLength();

		//Get type
		type = (VideoCodec::Type)packet->GetCodec();

		//Lost packets since last
		DWORD lost = 0;

		//If not first
		if (lastSeq!=RTPPacket::MaxExtSeqNum)
			//Calculate losts
			lost = seq-lastSeq-1;

		//Increase total lost count
		lostCount += lost;

		//Update last sequence number
		lastSeq = seq;

		//If lost some packets or still have not got an iframe
		if(lostCount || waitIntra)
		{
			//Check if we got listener and more than 1/2 second have elapsed from last request
			if (listener && getDifTime(&lastFPURequest)/1000>minFPUPeriod)
			{
				//Debug
				Debug("-Requesting FPU lost %d\n",lostCount);
				//Reset count
				lostCount = 0;
				//Request it
				listener->onRequestFPU();
				//Request also over rtp
				rtp.RequestFPU();
				//Update time
				getUpdDifTime(&lastFPURequest);
				//Waiting for refresh
				waitIntra = true;
			}
		}

		//Check if it is a redundant packet
		if (type==VideoCodec::RED)
		{
			//Get redundant packet
			auto red = std::static_pointer_cast<RTPRedundantPacket>(packet);
			//Get primary codec
			type = (VideoCodec::Type)red->GetPrimaryCodec();
			//Check it is not ULPFEC redundant packet
			if (type==VideoCodec::ULPFEC)
				//Skip
				continue;
			//Update primary redundant payload
			buffer = red->GetPrimaryPayloadData();
			size = red->GetPrimaryPayloadSize();
		}
		
		//Check codecs
		if ((videoDecoder==NULL) || (type!=videoDecoder->type))
		{
			//If we already got one
			if (videoDecoder!=NULL)
				//Delete it
				delete videoDecoder;

			//Create video decorder for codec
			videoDecoder = VideoCodecFactory::CreateDecoder(type);

			//Check
			if (videoDecoder==NULL)
			{
				Error("Error creando nuevo decodificador de video [%d]\n",type);
				//Next
				continue;
			}
		}

		//Check if we have lost the last packet from the previous frame by comparing both timestamps
		if (ts>frameTime)
		{
			Debug("-lost mark packet ts:%llu frameTime:%llu\n",ts,frameTime);
			//Try to decode what is in the buffer
			videoDecoder->DecodePacket(NULL,0,1,1);
			//Get picture
			BYTE *frame = videoDecoder->GetFrame();
			DWORD width = videoDecoder->GetWidth();
			DWORD height = videoDecoder->GetHeight();
			//Check values
			if (frame && width && height)
			{
				//Set frame size
				videoOutput->SetVideoSize(width,height);

				//Check if muted
				if (!muted)
					//Send it
					videoOutput->NextFrame(frame);
			}
		}
		
		//Update frame time
		frameTime = ts;
		
		//Decode packet
		if(!videoDecoder->DecodePacket(buffer,size,lost,packet->GetMark()))
		{
			//Check if we got listener and more than 1/2 seconds have elapsed from last request
			if (listener && getDifTime(&lastFPURequest)/1000>minFPUPeriod)
			{
				//Debug
				Log("-Requesting FPU decoder error\n");
				//Reset count
				lostCount = 0;
				//Request it
				listener->onRequestFPU();
				//Request also over rtp
				rtp.RequestFPU();
				//Update time
				getUpdDifTime(&lastFPURequest);
				//Waiting for refresh
				waitIntra = true;
			}
		}
		
		//Get decode time
		diff = getUpdDifTime(&now)/1000;
		
		//Update waited time
		decodeTimeAcu.Update(getTime(now)/1000,diff);

		//Check if it is the last packet of a frame
		if(packet->GetMark())
		{
			//One morw frame
			fpsAcu.Update(getTime(now)/1000,1);

			if (videoDecoder->IsKeyFrame())
				Debug("-Got Intra\n");
			
			//No frame time yet for next frame
			frameTime = (QWORD)-1;

			//Get picture
			BYTE *frame = videoDecoder->GetFrame();
			DWORD width = videoDecoder->GetWidth();
			DWORD height = videoDecoder->GetHeight();
			//Check values
			if (frame && width && height)
			{
				//Set frame size
				videoOutput->SetVideoSize(width,height);
				
				//Check if muted
				if (!muted)
					//Send it
					videoOutput->NextFrame(frame);
			}
			//Check if we got the waiting refresh
			if (waitIntra && videoDecoder->IsKeyFrame())
				//Do not wait anymore
				waitIntra = false;
			
			//Get deliver time
			diff = getUpdDifTime(&now)/1000;
		
			//Update waited time
			deliverTimeAcu.Update(getTime(now)/1000,diff);
			
			//Dump stats each 6 frames
			if (fpsAcu.GetAcumulated() % 60 == 0)
			{
				//Log
				UltraDebug("-VideoStream::RecVideo() fps [min:%.2Lf,max:%.2Lf,avg:%.2Lf] wait [min:%.2Lf,max:%.2Lf,avg:%.2Lf] enc [min:%.2Lf,max:%.2Lf,avg:%.2Lf] deliver [min:%.2Lf,max:%.2Lf,avg:%.2Lf]\n",
					fpsAcu.GetMinAvg()		,fpsAcu.GetMaxAvg()		, fpsAcu.GetInstantAvg(),
					waitTimeAcu.GetMinAvg()		,waitTimeAcu.GetMaxAvg()	, waitTimeAcu.GetInstantAvg(),
					decodeTimeAcu.GetMinAvg()	,decodeTimeAcu.GetMaxAvg()	, decodeTimeAcu.GetInstantAvg(),
					deliverTimeAcu.GetMinAvg()	,deliverTimeAcu.GetMaxAvg()	, deliverTimeAcu.GetInstantAvg()
				);
				//Reset min and max
				fpsAcu.ResetMinMax();
				waitTimeAcu.ResetMinMax();
				decodeTimeAcu.ResetMinMax();
				deliverTimeAcu.ResetMinMax();
			}
		}
	}

	//Delete encoder
	delete videoDecoder;

	Log("<RecVideo\n");

	return 1;
}

int VideoStream::SetMediaListener(MediaFrame::Listener *listener)
{
	//Set it
	this->mediaListener = listener;
	
	return 1;
}

int VideoStream::SendFPU()
{
	Debug(">SendFPU\n");
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
