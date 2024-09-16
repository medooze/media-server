/* 
 * File:   opusdecoder.cpp
 * Author: Sergio
 * 
 * Created on 14 de marzo de 2013, 11:19
 */

#include "OpusDecoder.h"
#include "log.h"

OpusDecoder::OpusDecoder()
{
	int error = 0;
	//Set type
	type = AudioCodec::OPUS;
	//Set rate
	rate = 48000;
	//Create decoder
	numChannels = 1;
	dec = opus_decoder_create(rate, numChannels, &error);
	//Check error
	if (!dec || error)
		Error("Could not open OPUS encoder");
}

DWORD OpusDecoder::TrySetRate(DWORD rate)
{
	int error = 0;
	//Try to create a new decored with that rate
	OpusDecoder *aux = opus_decoder_create(rate,1,&error);
	//If no error
	if (aux && !error)
	{
		//Destroy old one
		if (dec) opus_decoder_destroy(dec);
		//Get new decoded
		dec = aux;
		//Store new rate
		this->rate = rate;
	}
	
	//Return codec rate
	return this->rate;
}

OpusDecoder::~OpusDecoder()
{
	if (dec)
		opus_decoder_destroy(dec);
}

int OpusDecoder::Decode(const AudioFrame::const_shared& audioFrame)
{
	//Decode without FEC
	auto inLen = audioFrame->GetLength();
	uint8_t* in = const_cast<uint8_t*>(audioFrame->GetData());
	if (!in || !inLen)
		return 0;
	int outLen = rate*20/1000;
	auto audioBuffer = std::make_shared<AudioBuffer>(outLen, numChannels);
	int ret = opus_decode(dec, in, inLen, (opus_int16*)audioBuffer->GetData(), outLen, 0);
	//Check error
	if (ret<0)
	{
		Error("-Opus decode error [%d]\n",ret);
		return 0;
	}
	audioBufferQueue.push(std::move(audioBuffer));
	return 1;
}

AudioBuffer::shared OpusDecoder::GetDecodedAudioFrame()
{
	if(!audioBufferQueue.empty())
	{
		auto audioBuffer = audioBufferQueue.front();
		audioBufferQueue.pop();
		return audioBuffer;
	}
	return {};
}