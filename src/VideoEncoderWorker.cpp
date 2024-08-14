/* 
 * File:   VideoEncoderWorker.cpp
 * Author: Sergio
 * 
 * Created on 12 de agosto de 2014, 10:32
 */

#include "VideoEncoderWorker.h"
#include "log.h"
#include "tools.h"
#include "acumulator.h"
#include "VideoCodecFactory.h"

VideoEncoderWorker::VideoEncoderWorker() :
	bitrateAcu(1000),
	fpsAcu(1000),
	encodingTimeAcu(1000),
	capturingTimeAcu(1000)
{
	//Create objects
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);
}

VideoEncoderWorker::~VideoEncoderWorker()
{
	End();
	//Clean object
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

int VideoEncoderWorker::Init(VideoInput *input)
{
	//Store it
	this->input = input;
	//Done
	return true;
}

int VideoEncoderWorker::SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod, const Properties& properties)
{
	return SetVideoCodec(codec,GetWidth(mode),GetHeight(mode),fps,bitrate,intraPeriod,properties);
}

int VideoEncoderWorker::SetVideoCodec(VideoCodec::Type codec, int width, int height, int fps,int bitrate,int intraPeriod, const Properties& properties)
{
	Log("-VideoEncoderWorker::SetCodec() [%s,width:%d,height:%d,fps:%d,bitrate:%d,intraPeriod:%d]\n",VideoCodec::GetNameFor(codec),width,height,fps,bitrate,intraPeriod);

	//Check size
	if (!width || !height)
		//Error
		return Error("Wrong size\n");
	
	//Store parameters
	this->codec	  = codec;
	this->width	  = width;
	this->height	  = height;
	this->bitrate	  = bitrate;
	this->fps	  = fps;
	this->intraPeriod = intraPeriod;
        //Store properties
        this->properties  = properties;

	//Good
	return 1;
}

int VideoEncoderWorker::Start()
{
	Log("-VideoEncoderWorker::Start()\n");
	
	//Check
	if (!input)
		//Exit
		return Error("-VideoEncoderWorker::Start() Error: null video input");
	
	
	//Check if need to restart
	if (encoding)
		//Stop first
		Stop();

	//Start decoding
	encoding = 1;

	//launc thread
	createPriorityThread(&thread,startEncoding,this,0);

	return 1;
}

void * VideoEncoderWorker::startEncoding(void *par)
{
	//Get worker
	VideoEncoderWorker *worker = (VideoEncoderWorker *)par;
	//Block all signals
	blocksignals();
	//Run
	worker->Encode();
	//Exit
	return NULL;
}

int VideoEncoderWorker::Stop()
{
	Log(">VideoEncoderWorker::Stop()\n");

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

	Log("<VideoEncoderWorker::Stop()\n");

	return 1;
}

int VideoEncoderWorker::End()
{
	//Check if already decoding
	if (encoding)
		//Stop
		Stop();

	//Set null
	input = NULL;
	
	//Done
	return 1;
}

int VideoEncoderWorker::Encode()
{
	timeval lastFPU;
	
	DWORD num = 0;

	Log(">VideoEncoderWorker::Encode() [width:%d,height:%d,bitrate:%d,fps:%d,intra:%d]\n",width,height,bitrate,fps,intraPeriod);

	//Create encoder
	std::unique_ptr<VideoEncoder> videoEncoder(VideoCodecFactory::CreateEncoder(codec,properties));

	//Check
	if (!videoEncoder)
		//error
		return Error("Can't create video encoder\n");

	//Comrpobamos que tengamos video de entrada
	if (input == NULL)
		return Error("No video input");

	//Start vicedeo capture
	if (!input->StartVideoCapture(width,height,fps))
		return Error("Couldn't set video capture\n");


	//Set bitrate
	videoEncoder->SetFrameRate(fps,bitrate,intraPeriod);

	//Set initial size
 	videoEncoder->SetSize(width,height);
	
	//Capturing time init
	uint64_t captureTimeStart = getTimeMS();

	//Fist FPU
	gettimeofday(&lastFPU,NULL);

	//Mientras tengamos que capturar
	while(encoding)
	{
		//Capture video frame buffer
		auto pic = input->GrabFrame(0);

		//Check picture
		if (!pic)
			//Exit
			continue;

		//Get capturing time
		uint64_t captureTimeEnd = getTimeMS();

		//Update
		capturingTimeAcu.Update(captureTimeEnd, captureTimeEnd - captureTimeStart);
		
		//Check size
		if (pic->GetWidth() != width || pic->GetHeight() != height)
		{
			//Update size
			width	= pic->GetWidth();
			height	= pic->GetHeight();
			//Create encoder again
			videoEncoder.reset(VideoCodecFactory::CreateEncoder(codec, properties));
			//Reset bitrate
			videoEncoder->SetFrameRate(fps,bitrate,intraPeriod);
			//Set on the encoder
			videoEncoder->SetSize(width,height);
		}

		//Check if we need to send intra
		if (sendFPU)
		{
			//Do not send anymore
			sendFPU = false;
			//Do not send if just send one (100ms)
			if (getDifTime(&lastFPU)/100>100)
			{
				//Set it
				videoEncoder->FastPictureUpdate();
				//Update last FPU
				getUpdDifTime(&lastFPU);
			}
		}

		//Get time before encoding
		uint64_t encodeStartTime = captureTimeEnd;

		//Procesamos el frame
		VideoFrame *videoFrame = videoEncoder->EncodeFrame(pic);

		//If was failed
		if (!videoFrame)
			//Next
			continue;
		//One encoded frame more
		num++;

		//Get time after encoding
		uint64_t encodeEndTime = getTimeMS();

		//Calculate encoding time
		encodingTimeAcu.Update(encodeEndTime, encodeEndTime - encodeStartTime);
		
		//Increase frame counter
		fpsAcu.Update(encodeEndTime, 1);

		//Add frame size in bits to bitrate calculator
		bitrateAcu.Update(encodeEndTime, videoFrame->GetLength()*8);
		
		//Set clock rate
		videoFrame->SetClockRate(pic->GetClockRate());
		//Set frame timestamp
		videoFrame->SetTimestamp(pic->GetTimestamp());
		videoFrame->SetTime(pic->HasTime() ? pic->GetTime() : encodeStartTime);
		if (pic->HasSenderTime()) videoFrame->SetSenderTime(pic->GetSenderTime());

		// @todo Add New ticket to move the code that sets timing information into the encoder like we do for the decoder
		//
		// The VideoBuffer is decoded and its timestamp IS a presentation time
		// We are producing an encoded VideoFrame object that could have separate PTS/DTS
		// However we only ever encode without B-frames so in this case they are identical
		// but in general, the encoder should be the one to tell us what to use.
		videoFrame->SetPresentationTimestamp(pic->GetTimestamp());

		// Set duration to 0 indicating we dont know its actual value
		// We *could* delay the frame until the next one and use timestamps 
		// to calculate the duration however we dont want to pay that latency cost. 
		// We cant use the fps as there are cases where this is incorrect and some
		// things (shaka recording) require being able to know if this is reliable/correct
		// so we mark it as not set.
		videoFrame->SetDuration(0);

		//Set target bitrate and fps
		videoFrame->SetTargetBitrate(bitrate);
		videoFrame->SetTargetFps(fps);

		//Lock
		pthread_mutex_lock(&mutex);

		//Recalculate stats
		stats.timestamp			= encodeEndTime;
		stats.totalEncodedFrames	= num;
		stats.fps			= fpsAcu.GetInstant();
		stats.bitrate			= bitrateAcu.GetInstant();
		stats.maxEncodingTime		= encodingTimeAcu.GetMaxValueInWindow();
		stats.avgEncodingTime		= static_cast<uint16_t>(encodingTimeAcu.GetInstantMedia());
		stats.maxCapturingTime		= capturingTimeAcu.GetMaxValueInWindow();
		stats.avgCapturingTime		= static_cast<uint16_t>(capturingTimeAcu.GetInstantMedia());

		//For each listener
		for (auto &listener : listeners)
		{
			//If was not null
			if (listener)
				//Call listener
				listener->onMediaFrame(*videoFrame);
		}

		//unlock
		pthread_mutex_unlock(&mutex);

		//Update previus capture time
		captureTimeStart = getTimeMS();
		
	}

	//Terminamos de capturar
	input->StopVideoCapture();

	//Salimos
	Log("<VideoEncoderWorker::Encode()  [%d]\n",encoding);
	
	//Done
	return 1;
}

bool VideoEncoderWorker::AddListener(const MediaFrame::Listener::shared& listener)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Add to set
	listeners.insert(listener);

	//unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

bool VideoEncoderWorker::RemoveListener(const MediaFrame::Listener::shared& listener)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Search
	Listeners::iterator it = listeners.find(listener);

	//If found
	if (it!=listeners.end())
		//Erase it
		listeners.erase(it);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}


void VideoEncoderWorker::SendFPU()
{
	sendFPU = true;
}

VideoEncoderWorker::Stats VideoEncoderWorker::GetStats()
{
	//Lock
	pthread_mutex_lock(&mutex);

	Stats cloned = stats;

	//Unlock
	pthread_mutex_unlock(&mutex);

	return cloned;
}