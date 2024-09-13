/* 
 * File:   opusencoder.cpp
 * Author: Sergio
 * 
 * Created on 14 de marzo de 2013, 11:19
 */
#include "OpusEncoder.h"
#include "log.h"

OpusEncoder::OpusEncoder(const Properties &properties) 
	: audioFrame(std::make_shared<AudioFrame>(AudioCodec::OPUS))
{
	int error;
	//Set type
	type = AudioCodec::OPUS;
	//Rate
	rate = 48000;
	numChannels = 1;
	//Set number of input frames for codec
	numFrameSamples = rate * 20 / 1000;

	for (Properties::const_iterator it = properties.begin(); it != properties.end(); ++it)
		Debug("-OpusEncoder::OpusEncoder() | Setting property [%s:%s]\n", it->first.c_str(), it->second.c_str());

	//Set default mode
	mode = properties.GetProperty("opus.application.audio",false) ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO;

	//Open encoder
	enc = opus_encoder_create(rate, 1, mode, &error);
		
	//Check error
	if (!enc || error)
	{
		Error("Could not open OPUS encoder");
		enc = nullptr;
		return;
	}

	//Enable FEC
	opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(properties.GetProperty("opus.inbandfec",false)));

	config.SetSampleRate(rate);
	config.SetOutputChannelCount(numChannels);
	audioFrame->AllocateCodecConfig(config.GetSize());
	config.Serialize(audioFrame->GetCodecConfigData(), audioFrame->GetCodecConfigSize());
	
	audioFrame->DisableSharedBuffer();
}

DWORD OpusEncoder::TrySetRate(DWORD rate, DWORD numChannels)
{
	Log("-OpusEncoder::TrySetRate() [rate:%d,numChannels:%d]\n",rate,numChannels);
	int error = 0;
	//Try to create a new encoder with that rate
	OpusEncoder *aux = opus_encoder_create(rate, numChannels, mode, &error);
	//If no error
	if (aux && !error)
	{
		//Destroy old one
		if (enc) opus_encoder_destroy(enc);
		//Get new decoded
		enc = aux;
		//Store new rate
		this->rate = rate;
		this->numChannels = numChannels;
		//Set number of input frames for codec
		numFrameSamples = rate * 20 / 1000;
	}

	if (!enc) return 0;
	
	//Enable FEC
	opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));

	//Return new rate
	return this->rate;
}

OpusEncoder::~OpusEncoder()
{
	if (enc)
		opus_encoder_destroy(enc);
}

AudioFrame::shared OpusEncoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{
	if (!enc) return {};
	auto in = audioBuffer->GetData();
	int inLen = audioBuffer->GetNumChannels() * audioBuffer->GetNumSamples();
	int len = opus_encode(enc, in, inLen , audioFrame->GetData(), audioFrame->GetMaxMediaLength());

	if( len < 0)
	{
		Error("-OpusEncoder::Encode() encode error\n");
		return {};
	}
		
	audioFrame->SetLength(len);
	return audioFrame;
}

void OpusEncoder::SetConfig(DWORD rate, DWORD numChannels)
{
	config.SetSampleRate(rate);
	config.SetOutputChannelCount(numChannels);
	if(audioFrame->HasCodecConfig())
		audioFrame->ClearCodecConfig();
	audioFrame->AllocateCodecConfig(config.GetSize());
	config.Serialize(audioFrame->GetCodecConfigData(), audioFrame->GetCodecConfigSize());
}