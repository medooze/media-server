#include "g711.h"
#include <string.h>
#include "g711codec.h"

#define NUMFRAMES 160

PCMACodec::PCMACodec()
{
	type=AudioCodec::PCMA;
	numFrameSamples=NUMFRAMES;
	frameLength=NUMFRAMES;
}

PCMACodec::~PCMACodec()
{
}

int PCMACodec::Encode (WORD *in,int inLen,BYTE* out,int outLen)
{
	//Comprobamos las longitudes
	if (outLen<inLen)
		return 0;

	//Y codificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=linear2alaw(in[j]);

	return inLen;
}

int PCMACodec::Decode (BYTE *in,int inLen,WORD* out,int outLen)
{
	//Comprobamos las longitudes
	if (outLen<inLen)
		return 0;

	//Decodificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=alaw2linear(in[j]);

	return inLen;	
}
