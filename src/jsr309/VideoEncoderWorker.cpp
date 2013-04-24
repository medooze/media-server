/* 
 * File:   VideoEncoderWorker.cpp
 * Author: Sergio
 * 
 * Created on 2 de noviembre de 2011, 23:37
 */

#include "VideoEncoderWorker.h"
#include "log.h"
#include "RTPMultiplexer.h"

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

int VideoEncoderMultiplexerWorker::SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int qMin, int qMax,int intraPeriod)
{
	Log("-SetVideoCodec [%s,%d,%d,%d,%d,%d,%d]\n",VideoCodec::GetNameFor(codec),codec,fps,bitrate,qMin,qMax,intraPeriod);

	//Store parameters
	this->codec	= codec;
	this->mode	= mode;
	this->bitrate	= bitrate;
	this->fps	= fps;
	this->qMin	= qMin;
	this->qMax	= qMax;
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

int VideoEncoderMultiplexerWorker::Encode()
{
	timeval first;
	timeval prev;

	Log(">SendVideo [%d,%d,%d,%d,%d,%d,%d]\n",width,height,bitrate,fps,qMin,qMax,intraPeriod);

	//Creamos el encoder
	VideoEncoder* videoEncoder = VideoCodecFactory::CreateEncoder(codec);

	//Comprobamos que se haya creado correctamente
	if (videoEncoder == NULL)
		return Error("Can't create video encoder\n");

	//Comrpobamos que tengamos video de entrada
	if (input == NULL)
		return Error("No video input");

	//Iniciamos el tama�o del video
	if (!input->StartVideoCapture(width,height,fps))
		return Error("Couldn't set video capture\n");

	//Iniciamos el birate y el framerate
	videoEncoder->SetFrameRate((float)fps,bitrate,intraPeriod);

	//Iniciamos el tamama�o del encoder
 	videoEncoder->SetSize(width,height);
	
	//No wait for first
	DWORD frameTime = 0;

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
		frameTime = 1000/fps;

		//Set frame timestamp
		videoFrame->SetTimestamp(getDifTime(&first)/1000);

		//Set sending time of previous frame
		getUpdDifTime(&prev);

		//Send it smoothly
		SmoothFrame(videoFrame,frameTime);
	}

	Log("-SendVideo out of loop\n");

	//Terminamos de capturar
	input->StopVideoCapture();

	//Borramos el encoder
	delete videoEncoder;

	//Salimos
	Log("<SendVideo [%d]\n",encoding);

	//Exit
	pthread_exit(0);
}
