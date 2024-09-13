#include "g711.h"
#include <string.h>
#include "g711codec.h"

#define NUMFRAMES 160

PCMUEncoder::PCMUEncoder(const Properties &properties) 
	: audioFrame(std::make_shared<AudioFrame>(AudioCodec::PCMU))
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

AudioFrame::shared PCMUEncoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{
	const SWORD *in = audioBuffer->GetData();
	int inLen = audioBuffer->GetNumSamples() * audioBuffer->GetNumChannels();
	if(!in || inLen<=0) return nullptr;
	for (int j = 0; j < inLen ;j++)
	{
		uint8_t ulaw = linear2ulaw(in[j]);
		audioFrame->AppendMedia(&ulaw, 1); 
	}
	audioFrame->SetLength(inLen);
	return audioFrame;
}

int PCMUDecoder::Decode(const AudioFrame::const_shared& audioFrame)
{
	//Comprobamos las longitudes
	// inLen: compressed data length in bytes/samples one byte per sample for g711a, outLen: pcm data size in samples
	int inLen = audioFrame->GetLength();
	uint8_t* in = const_cast<uint8_t*>(audioFrame->GetData());
	if (!in || inLen <= 0)
		return 0;
	audioFrameInfo.first = in;
	audioFrameInfo.second = inLen;
	return 1;	
}

AudioBuffer::shared PCMUDecoder::GetDecodedAudioFrame()
{
	uint8_t* in = audioFrameInfo.first;
	int inLen = audioFrameInfo.second;

	auto audioBuffer = std::make_shared<AudioBuffer>(inLen, 1);
	for (int j = 0; j< inLen;j++)
	{
		if(audioBuffer->SetSampleAt(j, ulaw2linear(*in)))
			in++;
		else	
			return {};
	}
	return audioBuffer;
}

