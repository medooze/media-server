/* 
 * File:   VideoTranscoder.cpp
 * Author: Sergio
 * 
 * Created on 19 de marzo de 2013, 12:32
 */

#include "VideoTranscoder.h"
#include "videopipe.h"

VideoTranscoder::VideoTranscoder(std::wstring &name)
{
	//Store tag
	this->tag = name;

	//Not inited
	inited = false;
}

VideoTranscoder::~VideoTranscoder()
{
	//Check if ended properly
	if (inited)
		//End!!
		End();
}

int VideoTranscoder::Init()
{
	Log("-Init VideoTranscoder [%ls,encoder:%p,decoder:%p]\n",tag.c_str(),&encoder,&decoder);
	
	//Init pipe
	pipe.Init();
	//Start encoder
	encoder.Init(&pipe);
	//Star decoder
	decoder.Init(&pipe);
	//Inited
	inited = true;
	//OK
	return 1;
}
int VideoTranscoder::SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties& properties)
{
	return encoder.SetCodec(codec,mode,fps,bitrate,intraPeriod,properties);
}
int VideoTranscoder::End()
{
	Log("-End VideoTranscoder [%ls]\n",tag.c_str());
	//End encoder and decoder
	encoder.End();
	decoder.End();
	//End pipe
	pipe.End();
	//Not inited
	inited = false;
	//OK
	return 1;
}

void VideoTranscoder::AddListener(Joinable::Listener *listener)
{
	encoder.AddListener(listener);
}

void VideoTranscoder::Update()
{
	encoder.Update();
}

void VideoTranscoder::SetREMB(DWORD estimation)
{
	encoder.SetREMB(estimation);
}

void VideoTranscoder::RemoveListener(Joinable::Listener *listener)
{
	encoder.RemoveListener(listener);
}

void VideoTranscoder::onRTPPacket(RTPPacket &packet)
{
	decoder.onRTPPacket(packet);
}
void VideoTranscoder::onResetStream()
{
	decoder.onResetStream();
}
void VideoTranscoder::onEndStream()
{
	decoder.onEndStream();
}

int VideoTranscoder::Attach(Joinable *join)
{
	decoder.Attach(join);
}

int VideoTranscoder::Dettach()
{
	decoder.Dettach();
}