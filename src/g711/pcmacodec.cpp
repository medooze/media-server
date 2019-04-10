#include "g711.h"
#include <string.h>
#include "g711codec.h"

#define NUMFRAMES 160

PCMAEncoder::PCMAEncoder(const Properties &properties)
{
	type=AudioCodec::PCMA;
	numFrameSamples=NUMFRAMES;
}

PCMAEncoder::~PCMAEncoder()
{
}

PCMADecoder::PCMADecoder()
{
	type=AudioCodec::PCMA;
	numFrameSamples=NUMFRAMES;
}

PCMADecoder::~PCMADecoder()
{

}

int PCMAEncoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	//Comprobamos las longitudes
	if (outLen<inLen)
		return 0;

	//Y codificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=linear2alaw(in[j]);

	return inLen;
}

int PCMADecoder::Decode (const BYTE *in,int inLen,SWORD* out,int outLen)
{
	//Comprobamos las longitudes
	if (outLen<inLen)
		return 0;

	//Decodificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=alaw2linear(in[j]);

	return inLen;	
}
