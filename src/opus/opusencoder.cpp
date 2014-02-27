/* 
 * File:   opusencoder.cpp
 * Author: Sergio
 * 
 * Created on 14 de marzo de 2013, 11:19
 */
#include "opusencoder.h"
#include "log.h"

OpusEncoder::OpusEncoder(const Properties &properties)
{
	int error;
	//Set type
	type = AudioCodec::OPUS;
	//Rate
	rate = 48000;
	//Set number of input frames for codec
	numFrameSamples = rate * 20 / 1000;
	//Set default mode
	mode = OPUS_APPLICATION_VOIP;

	//Check speex quality overrride
	Properties::const_iterator it = properties.find(std::string("opus.applicaion.audio"));

	//If found
	if (it!=properties.end())
		//Update it
		mode = OPUS_APPLICATION_AUDIO;
	
	//Open encoder
	enc = opus_encoder_create(rate, 1, mode, &error);
		
	//Check error
	if (!enc || error)
		Error("Could not open OPUS encoder");

	//Enable FEC
	opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));
}

DWORD OpusEncoder::TrySetRate(DWORD rate)
{
	int error = 0;
	//Try to create a new encoder with that rate
	OpusEncoder *aux = opus_encoder_create(rate, 1, mode, &error);
	//If no error
	if (aux && !error)
	{
		//Destroy old one
		if (enc) opus_encoder_destroy(enc);
		//Get new decoded
		enc = aux;
		//Store new rate
		this->rate = rate;
		//Set number of input frames for codec
		numFrameSamples = rate * 20 / 1000;
	}

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

int OpusEncoder::Encode(SWORD *in,int inLen,BYTE* out,int outLen)
{
	return opus_encode(enc,in,inLen,out,outLen);
}
