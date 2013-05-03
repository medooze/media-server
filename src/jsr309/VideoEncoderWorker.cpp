/* 
 * File:   VideoEncoderWorker.cpp
 * Author: Sergio
 * 
 * Created on 2 de noviembre de 2011, 23:37
 */

#include "VideoEncoderWorker.h"
#include "log.h"
#include "tools.h"
#include "RTPMultiplexer.h"
#include "acumulator.h"

VideoEncoderMultiplexerWorker::VideoEncoderMultiplexerWorker() : RTPMultiplexerSmoother()
{
	//Nothing
	input = NULL;
	encoding = false;
	sendFPU = false;
	codec = (VideoCodec::Type)-1;
	//Create objects
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);
}

VideoEncoderMultiplexerWorker::~VideoEncoderMultiplexerWorker()
{
	End();
	//Clean object
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

int VideoEncoderMultiplexerWorker::Init(VideoInput *input)
{
	//Store it
	this->input = input;
}

int VideoEncoderMultiplexerWorker::SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod)
{
	Log("-SetVideoCodec [%s,%d,%d,%d,%d,]\n",VideoCodec::GetNameFor(codec),codec,fps,bitrate,intraPeriod);

	//Store parameters
	this->codec	= codec;
	this->mode	= mode;
	this->bitrate	= bitrate;
	this->fps	= fps;
	this->intraPeriod = intraPeriod;

	//Get width and height
	width = GetWidth(mode);
	height = GetHeight(mode);

	//Check size
	if (!width || !height)
		//Error
		return Error("Unknown video mode\n");

	//Check if we are already encoding
	if (!listeners.empty())
	{
		//Check if need to restart
		if (encoding)
			//Stop first
			Stop();
		//And start
		Start();
	}
	
	//Exit
	return 1;
}

int VideoEncoderMultiplexerWorker::Start()
{
	//Check
	if (!input)
		//Exit
		return Error("null video input");
	
	//Check if need to restart
	if (encoding)
		//Stop first
		Stop();

	//Start smoother
	RTPMultiplexerSmoother::Start();

	//Start decoding
	encoding = 1;

	//launc thread
	createPriorityThread(&thread,startEncoding,this,0);

	return 1;
}

void * VideoEncoderMultiplexerWorker::startEncoding(void *par)
{
	Log("VideoEncoderMultiplexerWorkerThread [%d]\n",getpid());
	//Get worker
	VideoEncoderMultiplexerWorker *worker = (VideoEncoderMultiplexerWorker *)par;
	//Block all signals
	blocksignals();
	//Run
	pthread_exit((void *)worker->Encode());
}

int VideoEncoderMultiplexerWorker::Stop()
{
	Log(">Stop VideoEncoderMultiplexerWorker\n");

	//If we were started
	if (encoding)
	{
		//Stop
		encoding=0;

		//Cancel and frame grabbing
		input->CancelGrabFrame();

		//Cancel sending
		pthread_cond_signal(&cond);

		//Esperamos
		pthread_join(thread,NULL);
	}

	//Stop smoother
	RTPMultiplexerSmoother::Stop();

	Log("<Stop VideoEncoderMultiplexerWorker\n");

	return 1;
}

int VideoEncoderMultiplexerWorker::End()
{
	//Check if already decoding
	if (encoding)
		//Stop
		Stop();

	//Set null
	input = NULL;
}

void VideoEncoderMultiplexerWorker::AddListener(Listener *listener)
{
	//Check if we were already encoding
	if (listener && !encoding && codec!=-1)
		//Start encoding;
		Start();
	//Add the listener
	RTPMultiplexer::AddListener(listener);
}

void VideoEncoderMultiplexerWorker::RemoveListener(Listener *listener)
{
	//Remove the listener
	RTPMultiplexer::RemoveListener(listener);
	//If there are no more
	if (listeners.empty())
		//Stop encoding
		Stop();
}

void VideoEncoderMultiplexerWorker::Update()
{
	//Sedn FPU
	sendFPU = true;
}

void VideoEncoderMultiplexerWorker::SetREMB(int estimation)
{
	//Check limit with comfigured bitrate
	if (estimation>bitrate)
		//Do nothing
		return;
	//Set bitrate limit
	videoBitrateLimit = estimation;
	//Set limit of bitrate to 1 second;
	videoBitrateLimitCount = fps;
	//Exit
	return;
}

int VideoEncoderMultiplexerWorker::Encode()
{
	timeval first;
	timeval prev;
	DWORD num = 0;
	QWORD overslept = 0;

	Acumulator bitrateAcu(1000);
	Acumulator fpsAcu(1000);

	Log(">SendVideo [width:%d,size:%d,bitrate:%d,fps:%d,intra:%d]\n",width,height,bitrate,fps,intraPeriod);

	//Creamos el encoder
	VideoEncoder* videoEncoder = VideoCodecFactory::CreateEncoder(codec);

	//Comprobamos que se haya creado correctamente
	if (videoEncoder == NULL)
		//error
		return Error("Can't create video encoder\n");

	//Comrpobamos que tengamos video de entrada
	if (input == NULL)
		return Error("No video input");

	//Iniciamos el tama�o del video
	if (!input->StartVideoCapture(width,height,fps))
		return Error("Couldn't set video capture\n");

	//Start at 80%
	int current = bitrate*0.8;

	//Iniciamos el birate y el framerate
	videoEncoder->SetFrameRate((float)fps,current,intraPeriod);

	//No wait for first
	QWORD frameTime = 0;

	//Iniciamos el tamama�o del encoder
 	videoEncoder->SetSize(width,height);
	
	//The time of the first one
	gettimeofday(&first,NULL);

	//The time of the previos one
	gettimeofday(&prev,NULL);

	//Started
	Log("-Sending video\n");

	//Mientras tengamos que capturar
	while(encoding)
	{
		//Nos quedamos con el puntero antes de que lo cambien
		BYTE *pic=input->GrabFrame();

		//Check picture
		if (!pic)
			//Exit
			continue;

		//Check if we need to send intra
		if (sendFPU)
		{
			//Log
			Log("-FastPictureUpdate\n");
			//Set it
			videoEncoder->FastPictureUpdate();
			//Do not send anymore
			sendFPU = false;
		}
		//Calculate target bitrate
		int target = current;

		//Check temporal limits for estimations
		if (bitrateAcu.IsInWindow())
		{
			//Get real sent bitrate during last second and convert to kbits (*1000/1000)
			DWORD instant = bitrateAcu.GetInstantAvg();
			//Check if are not in quarentine period or sending below limits
			if (videoBitrateLimitCount || instant<videoBitrateLimit)
				//Increase a 8% each second o 10kbps
				target += fmax(target*0.08,10000)/fps+1;
			else
				//Calculate decrease rate and apply it
				target = videoBitrateLimit;
		}

		//check max bitrate
		if (target>bitrate)
			//Set limit to max bitrate
			target = bitrate;

		//Check limits counter
		if (videoBitrateLimitCount>0)
			//One frame less of limit
			videoBitrateLimitCount--;

		//Check if we have a new bitrate
		if (target && target!=current)
		{
			//Reset bitrate
			videoEncoder->SetFrameRate(fps,target,intraPeriod);
			//Upate current
			current = target;
		}

		//Procesamos el frame
		VideoFrame *videoFrame = videoEncoder->EncodeFrame(pic,input->GetBufferSize());

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
			calcAbsTimeoutNS(&ts,&prev,frameTime-overslept);
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
		
		//Set next one
		frameTime = 1000000/fps;

		//Set frame timestamp
		videoFrame->SetTimestamp(getDifTime(&first)/1000);

		//Set sending time of previous frame
		getUpdDifTime(&prev);

		//Send it smoothly
		SmoothFrame(videoFrame,frameTime/1000);
	}

	Log("-SendVideo out of loop\n");

	//Terminamos de capturar
	input->StopVideoCapture();

	//Check
	if (videoEncoder)
		//Borramos el encoder
		delete videoEncoder;

	//Salimos
	Log("<SendVideo [%d]\n",encoding);

	//Exit
	pthread_exit(0);
}
