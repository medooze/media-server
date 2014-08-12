/* 
 * File:   VideoEncoderWorker.cpp
 * Author: Sergio
 * 
 * Created on 2 de noviembre de 2011, 23:37
 */

#include "VideoEncoderWorkerMultiplexer.h"
#include "log.h"
#include "tools.h"
#include "RTPMultiplexer.h"
#include "acumulator.h"

VideoEncoderMultiplexerWorker::VideoEncoderMultiplexerWorker() : RTPMultiplexerSmoother(), VideoEncoderWorker()
{
}

VideoEncoderMultiplexerWorker::~VideoEncoderMultiplexerWorker()
{
}

int VideoEncoderMultiplexerWorker::Init(VideoInput *input)
{
	//Init encoder
	return VideoEncoderWorker::Init(input);
}

int VideoEncoderMultiplexerWorker::SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod, const Properties& properties)
{
	//Set codec properties
	if (!VideoEncoderWorker::SetCodec(codec,mode,fps,bitrate,intraPeriod,properties))
		//Error
		return 0;

	//Check if we are already encoding
	if (!listeners.empty())
		//Start
		Start();
	
	//Exit
	return 1;
}

int VideoEncoderMultiplexerWorker::Start()
{
	//Start smoother
	RTPMultiplexerSmoother::Start();
	//Start encoder
	VideoEncoderWorker::Start();
	
	return 1;
}

int VideoEncoderMultiplexerWorker::Stop()
{
	//Stop encoder
	VideoEncoderWorker::Stop();
	//Stop smoother
	RTPMultiplexerSmoother::Stop();

	return 1;
}

int VideoEncoderMultiplexerWorker::End()
{
	//Stop
	return Stop();
}

void VideoEncoderMultiplexerWorker::AddListener(Listener *listener)
{
	//Check if we were already encoding
	if (listener && !IsEncoding())
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
	SendFPU();
}

void VideoEncoderMultiplexerWorker::SetREMB(int estimation)
{
	//Set bitrate limit
	SetTemporalBitrateLimit(estimation);
}

void VideoEncoderMultiplexerWorker::FlushRTXPackets()
{
	
}

void VideoEncoderMultiplexerWorker::SmoothFrame(const VideoFrame *videoFrame,DWORD sendingTime)
{
	RTPMultiplexerSmoother::SmoothFrame(videoFrame,sendingTime);
}
