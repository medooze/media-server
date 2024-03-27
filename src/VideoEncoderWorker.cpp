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

VideoEncoderWorker::VideoEncoderWorker() 
	: bitrateAcu(1000)
	, fpsAcu(1000)
{
	//Create objects
	pthread_mutex_init(&mutex,NULL);
}

VideoEncoderWorker::~VideoEncoderWorker()
{
	End();
	//Clean object
	pthread_mutex_destroy(&mutex);
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
	//Init limits
	this->bitrateLimit	= bitrate;
	this->bitrateLimitCount	= fps;
        //Store properties
        this->properties  = properties;

	this->frameTime = 1E6/fps;
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

		//Cancel and frame grabbing (also wakes up the thread loop)
		input->CancelGrabFrame();

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

	Log(">VideoEncoderWorker::Encode() [width:%d,height:%d,bitrate:%d,fps:%d,intra:%d]\n",width,height,bitrate,fps,intraPeriod);

	//Creamos el encoder
	videoEncoder = std::unique_ptr<VideoEncoder>(VideoCodecFactory::CreateEncoder(codec,properties));

	//Comprobamos que se haya creado correctamente
	if (!videoEncoder)
		//error
		return Error("Can't create video encoder\n");

	//Comrpobamos que tengamos video de entrada
	if (input == NULL)
		return Error("No video input");

	//Iniciamos el tama�o del video
	if (!input->StartVideoCapture(width,height,fps))
		return Error("Couldn't set video capture\n");


	//Set bitrate
	videoEncoder->SetFrameRate(fps,bitrate,intraPeriod);

	//Iniciamos el tamama�o del encoder
 	videoEncoder->SetSize(width,height);
	
	//The time of the first one
	gettimeofday(&first,NULL);

	//Fist FPU
	gettimeofday(&lastFPU,NULL);

	//Mientras tengamos que capturar
	while(encoding)
	{
		// Get the next video frame that was decoded as soon as it is ready
		// The frameTime/1000 timeout is just to allow safety for checking
		// the "encoding" bool in case of a bug prevents deadlock and not
		// really required. The generation of new frames is not driven by a
		// local timer but instead driven by the incoming frames
		// so the produced frames operate on the same clock as the source
		// frames.
		//
		// Frame rate transrating is handled by looking at timestamps so it
		// is also driven by the original clock (but with some jitter)
		auto pic = input->GrabFrame((unsigned long)frameTime/1000);
		if (!pic)
			continue;

		HandleFrame(pic);

		//Dump statistics
		if (num && ((num%fps*10)==0))
		{
			//Debug("-Send bitrate current=%d avg=%llf rate=[%llf,%llf] fps=[%llf,%llf] limit=%d\n",current,bitrateAcu.GetInstantAvg()/1000,bitrateAcu.GetMinAvg()/1000,bitrateAcu.GetMaxAvg()/1000,fpsAcu.GetMinAvg(),fpsAcu.GetMaxAvg(),bitrateLimit);
			bitrateAcu.ResetMinMax();
			fpsAcu.ResetMinMax();
		}
		num++;
	}

	//Terminamos de capturar
	input->StopVideoCapture();

	//Salimos
	Log("<VideoEncoderWorker::Encode()  [%d]\n",encoding);
	
	//Done
	return 1;
}

void VideoEncoderWorker::HandleFrame(VideoBuffer::const_shared pic)
{
	// This only works if all incoming frames have timestamps
	assert(pic->HasTimestamp());

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

	// synchronize on the first frame
	if (!lastFrame)
	{
		lastFrame = pic;
		firstEncodedTimestamp = pic->GetTimestamp();
		EncodeFrame(pic, firstEncodedTimestamp);
		++encodedFrames;

		return;
	}

	// Calculate the next timestamp to encode (Dont just += frameTime here as we want to be able to cope with fractional fps)
	// @todo The fps really should be originally either a float OR as often represented in video code as numerator/denominator
	// @todo For now we may get issues with accuracy as encodedFrames grows larger
	for (uint64_t nextEncodedTimestamp = firstEncodedTimestamp + static_cast<uint64_t>(frameTime * encodedFrames);
		nextEncodedTimestamp <= pic->GetTimestamp();
		++encodedFrames)
	{
		// Either lastFrame ir pic depending on timestamp
		VideoBuffer::const_shared frameToEncode;
		if (nextEncodedTimestamp < pic->GetTimestamp())
		{
			assert(nextEncodedTimestamp >= lastFrame->GetTimestamp());
			assert(nextEncodedTimestamp < pic->GetTimestamp());
			frameToEncode = lastFrame;
		}
		else
		{
			assert(nextEncodedTimestamp == pic->GetTimestamp());
			frameToEncode = pic;
		}
		
		// Note: If this fails for some reason we will simply skip sending
		// that frame
		EncodeFrame(frameToEncode, nextEncodedTimestamp);
		lastEncodedTimestamp = nextEncodedTimestamp;
	}
	lastFrame = pic;
}

bool VideoEncoderWorker::EncodeFrame(VideoBuffer::const_shared frame, uint64_t timestamp)
{
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

	//Procesamos el frame
	VideoFrame *videoFrame = videoEncoder->EncodeFrame(frame);
	if (!videoFrame)
		return false;

	//Increase frame counter
	fpsAcu.Update(getTime()/1000,1);


	//Add frame size in bits to bitrate calculator
	bitrateAcu.Update(getDifTime(&first)/1000,videoFrame->GetLength()*8);
	
	// @todo Like we require tiumestamp set, I think we can rely on this as well right?
	assert(frame->HasTime());
	assert(frame->HasClockRate());
	auto now = getDifTime(&first)/1000;

	videoFrame->SetClockRate(frame->HasClockRate() ? frame->GetClockRate() : 90000);
	videoFrame->SetTimestamp(timestamp);
	videoFrame->SetTime(frame->HasTime() ? frame->GetTime() : now);

	videoFrame->SetDuration(timestamp - lastEncodedTimestamp);
	//@todo bcost fix this
	// Set duration to 0 indicating we dont know its actual value
	// We *could* delay the frame until the next one and use timestamps 
	// to calculate the duration however we dont want to pay that latency cost. 
	// We cant use the fps as there are cases where this is incorrect and some
	// things (shaka recording) require being able to know if this is reliable/correct
	// so we mark it as not set.
	//videoFrame->SetDuration(0);


	//Set target bitrate and fps
	videoFrame->SetTargetBitrate(bitrate);
	videoFrame->SetTargetFps(fps);

	//Lock
	pthread_mutex_lock(&mutex);

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

	
/*
	//Calculate sending times based on bitrate
	DWORD sendingTime = videoFrame->GetLength()*8/bitrate;

	//Adjust to maximum time
	if (sendingTime>frameTime/1000)
		//Cap it
		sendingTime = frameTime/1000;

			//If it was a I frame
			if (videoFrame->IsIntra())
		//Clean rtp rtx buffer
		FlushRTXPackets();

	//Send it smoothly
	SmoothFrame(videoFrame,sendingTime);
*/

}


int VideoEncoderWorker::SetTemporalBitrateLimit(int estimation)
{
	//Set bitrate limit
	bitrateLimit = estimation/1000;
	//Set limit of bitrate to 1 second;
	bitrateLimitCount = fps;
	//Exit
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
