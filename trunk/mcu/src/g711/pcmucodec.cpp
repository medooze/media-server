#include "g711.h"
#include <string.h>
#include "g711codec.h"

#define NUMFRAMES 160

PCMUCodec::PCMUCodec()
{
	type=AudioCodec::PCMU;
	numFrameSamples=NUMFRAMES;
	frameLength=NUMFRAMES;
}

PCMUCodec::~PCMUCodec()
{
}

int PCMUCodec::Encode (WORD *in,int inLen,BYTE* out,int outLen)
{
	//Comprobamos las longitudes
	if (outLen<inLen)
		return 0;

	//Y codificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=linear2ulaw(in[j]);

	return inLen;
}

int PCMUCodec::Decode (BYTE *in,int inLen,WORD* out,int outLen)
{
	//Comprobamos las longitudes
	if (outLen<inLen)
		return 0;

	//Decodificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=ulaw2linear(in[j]);

	return inLen;	
}
