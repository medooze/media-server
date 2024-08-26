#include "g711.h"
#include <string.h>
#include "g711codec.h"

#define NUMFRAMES 160

PCMUEncoder::PCMUEncoder(const Properties &properties)
{
	type=AudioCodec::PCMU;
	numFrameSamples=NUMFRAMES;
}

PCMUEncoder::~PCMUEncoder()
{
}

PCMUDecoder::PCMUDecoder()
{
	type=AudioCodec::PCMU;
	numFrameSamples=NUMFRAMES;
}

PCMUDecoder::~PCMUDecoder()
{
}

int PCMUEncoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	//Comprobamos las longitudes
	if (outLen<inLen)
		return 0;

	//Y codificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=linear2ulaw(in[j]);

	return inLen;
}

int PCMUDecoder::Decode(const AudioFrame::const_shared& audioFrame, SWORD* out,int outLen)
{
	// Comprobamos las longitudes
	// inLen: compressed data length in bytes/samples one byte per sample for g711a, outLen: pcm data size in samples
	int inLen = audioFrame->GetLength();
	const uint8_t* in = audioFrame->GetData();
	if (outLen<inLen || !in)
		return 0;

	//Decodificamos
	for (int j = 0; j< inLen ;j++)
		out[j]=ulaw2linear(in[j]);

	return inLen;	
}
