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
	Log("-VideoEncoderWorker[%p]::VideoEncoderWorker()\n", this);

	//Create objects
	pthread_mutex_init(&mutex,NULL);
}

VideoEncoderWorker::~VideoEncoderWorker()
{
	Log("-VideoEncoderWorker[%p]::~VideoEncoderWorker()\n", this);
	End();
	//Clean object
	pthread_mutex_destroy(&mutex);
}

int VideoEncoderWorker::Init(VideoInput *input)
{
	Log("-VideoEncoderWorker[%p]::Init()\n", this);

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
	Log("-VideoEncoderWorker[%p]::SetCodec() [%s,width:%d,height:%d,fps:%d,bitrate:%d,intraPeriod:%d]\n",this,VideoCodec::GetNameFor(codec),width,height,fps,bitrate,intraPeriod);

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
	Log("-VideoEncoderWorker[%p]::Start()\n", this);
	
	//Check
	if (!input)
		//Exit
		return Error("-VideoEncoderWorker[%p]::Start() Error: null video input", this);
	
	
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
	Log(">VideoEncoderWorker[%p]::Stop()\n", this);

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

	Log("<VideoEncoderWorker[%p]::Stop()\n",this);

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

	Log(">VideoEncoderWorker[%p]::Encode() [width:%d,height:%d,bitrate:%d,fps:%d,intra:%d]\n",this,width,height,bitrate,fps,intraPeriod);

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
			//Debug("-VideoEncoderWorker[%p]::Encode() Send bitrate avg=%llf rate=[%llf,%llf] fps=[%llf,%llf] limit=%d\n",this,bitrateAcu.GetInstantAvg()/1000,bitrateAcu.GetMinAvg()/1000,bitrateAcu.GetMaxAvg()/1000,fpsAcu.GetMinAvg(),fpsAcu.GetMaxAvg(),bitrateLimit);
			bitrateAcu.ResetMinMax();
			fpsAcu.ResetMinMax();
		}
		num++;
	}

	//Terminamos de capturar
	input->StopVideoCapture();

	//Salimos
	Log("<VideoEncoderWorker[%p]::Encode()  [%d]\n",this,encoding);
	
	//Done
	return 1;
}

void VideoEncoderWorker::HandleFrame(VideoBuffer::const_shared pic)
{
	// This only works if all incoming frames have timestamps
	assert(pic->HasTimestamp());
	assert(pic->HasClockRate());
	assert(pic->GetClockRate() != 0);

	// Recreate the encoder if the resolution changes
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

	// synchronize on the first frame or on any problems seen with the incoming clock
	if (!lastFrame 

		// If the clock rate changes then we need to resync
		|| currentClockRate != pic->GetClockRate()

		// Something wrong with the current clock rate
		|| currentClockRate == 0

		// Time should not go backwards, if it does something is wrong and we should resync
		|| pic->GetTimestamp() < lastFrame->GetTimestamp()
		
		// Time should not jump by a significant amount (lets say 20 sec), if it does something is wrong and we should resync
		|| pic->GetTimestamp() > (lastFrame->GetTimestamp() + (currentClockRate * 20))
		)
	{
		Debug("VideoEncoderWorker[%p]::HandleFrame() Syncing encode clock from decoded frame with timestamp: %llu[%p], clock rate: %u, previous sync was: %llu and last seen frame: %llu\n", this, pic->GetTimestamp(), pic.get(), pic->GetClockRate(), firstEncodedTimestamp, (lastFrame ? lastFrame->GetTimestamp() : 0LLU));
		lastFrame = pic;
		firstEncodedTimestamp = pic->GetTimestamp();
		currentClockRate = pic->GetClockRate();
		assert(currentClockRate != 0);

		EncodeFrame(pic, firstEncodedTimestamp);
		++encodedFrames;
		return;
	}

	auto ToEncodedTimestamp = [&] (size_t frameIndex) -> uint64_t {
		// The fps is currently an int, other video related software often store fps as a int(numerator)/int(denominator)
		// For now we will treat the period as a double in the calculations so we can handle fps values like 30fps with 
		// a period of 0.03333... and have accurate integral timestamps that change over time as necessary
		//
		// Note: This method will have issues when the frameIndex gets super large but so will the uint64_t timestamps
		double  encodedFramePeriod = currentClockRate / (double)fps;
		return firstEncodedTimestamp + (uint64_t)(encodedFramePeriod * frameIndex);
	};

	// When we get a new frame it may produce 0..N encoded frames depending on where we are in 
	// the encoded frame timeline and the fps of the ingress vs encoded video
	for (
		uint64_t nextEncodedTimestamp = ToEncodedTimestamp(encodedFrames);
		nextEncodedTimestamp <= pic->GetTimestamp();
		nextEncodedTimestamp = ToEncodedTimestamp(encodedFrames)
		)
	{
		// We will duplicate from the nearest frame.
		uint64_t midPoint = (lastFrame->GetTimestamp() + pic->GetTimestamp()) / 2;
		if (nextEncodedTimestamp < midPoint)
		{
			EncodeFrame(lastFrame, nextEncodedTimestamp);
		}
		else
		{
			EncodeFrame(pic, nextEncodedTimestamp);
		}
		
		// Note: If this fails for some reason we will simply skip sending
		// that frame
		lastEncodedTimestamp = nextEncodedTimestamp;
		++encodedFrames;
	}
	lastFrame = pic;

	//float overallFps = (double)encodedFrames / ((getTime() - firstTime) / 1000000.0);
	//Debug("VideoEncoderWorker[%p]: Encoded %u frames from this ingress packet: %llu overall fps: %f (target:%d), frames: %lu\n", this, encodedThisTick, pic->GetTimestamp(), overallFps, fps, (unsigned long)encodedFrames);
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
	auto estart = getTime();
	VideoFrame *videoFrame = videoEncoder->EncodeFrame(frame);
	if (!videoFrame)
	{
		return false;
	}
	auto eend = getTime();
	// Debug("VideoEncoderWorker[%p]: Encoding frame: %llu[%p] returning videoFrame: %llu[%p] setting timestamp to: %llu\n", this, frame->GetTimestamp(), frame.get(), videoFrame->GetTimestamp(), videoFrame, timestamp);

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

	//Debug("VideoEncoderWorker[%p]: Encoded video frame: %llu[%p] from latest: %llu[%p]\n", this, videoFrame->GetTimestamp(), videoFrame, frame->GetTimestamp(), frame);
	UpdateStats(frame, videoFrame, eend - estart);

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

	return true;

	
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

void VideoEncoderWorker::UpdateStats(const VideoBuffer::shared& sourceFrame, const VideoFrame* encodedFrame, uint64_t encodeDuration)
{
	auto nowt = getTime();
	if (!tslog)
	{
		std::ostringstream ss;
		ss << "encode_tslog_video_" << nowt << "_" << sourceFrame->GetHeight() << "p_" << (void*)this << ".csv";
		tslog.open(ss.str().c_str());
		tslog << "time"
			<< ",clockrate"
			<< ",timestamp"
			<< ",encodedFrames"
			<< ",encodeDuration"
			<< ",overallFps"
			<< ",fromTimestamp"
			<< ",intra"
			<< "\n";
	}

	float overallFps = (double)encodedFrames / ((nowt - firstTime) / 1000000.0);
	tslog << nowt 
		<< "," << encodedFrame->GetClockRate() 
		<< "," << encodedFrame->GetTimestamp() 
		<< "," << encodedFrames 
		<< "," << encodeDuration 
		<< "," << overallFps 
		<< "," << sourceFrame->GetTimestamp() 
		<< "," << (encodedFrame->IsIntra() ? "1" : "0") 
		<< "\n";
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
