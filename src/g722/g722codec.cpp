/* 
 * File:   NellyCodec.cpp
 * Author: Sergio
 * 
 * Created on 7 de diciembre de 2011, 23:29
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "g722codec.h"
#include "fifo.h"
#include "log.h"

G722Encoder::G722Encoder(const Properties &properties)
{
	///Set type
	type = AudioCodec::G722;
	// 20 ms @ 16 Khz = 320 samples
	numFrameSamples = 20 * 16;
	//Init encoder
	g722_encode_init(&encoder,64000, 2);
}

int G722Encoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	return g722_encode(&encoder,out,in,inLen);
}

G722Decoder::G722Decoder()
{
	///Set type
	type = AudioCodec::G722;
	// 20 ms @ 16 Khz = 320 samples
	numFrameSamples = 20 * 16;
	//Init decoder
	g722_decode_init(&decoder, 64000, 2);
}


int G722Decoder::Decode(const BYTE *in, int inLen, SWORD* out, int outLen)
{
	return g722_decode(&decoder,out,in,inLen);
}

