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
	//Set number of input frames for codec
	numFrameSamples = 160;
	//Open encoder
	enc = opus_encoder_create(8000, 1, OPUS_APPLICATION_VOIP, &error);
	//Check error
	if (!enc || error)
		Error("Could not open OPUS encoder");
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
