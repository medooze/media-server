#include "g711.h"
#include <string.h>
#include "g711codec.h"

#define NUMFRAMES 160

PCMAEncoder::PCMAEncoder(const Properties &properties) : audioFrame(AudioCodec::PCMA)
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

AudioFrame* PCMAEncoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{
	const SWORD *in = audioBuffer->GetData();
	int inLen = audioBuffer->GetNumSamples() * audioBuffer->GetNumChannels();
	if(!in || inLen<=0) return nullptr;
	for (int j = 0; j< inLen ;j++)
	{
		uint8_t alaw = linear2alaw(in[j]);
		audioFrame.AppendMedia(&alaw, 1); 
	}
	audioFrame.SetLength(inLen);
	return &audioFrame;
}


int PCMADecoder::Decode(const AudioFrame::const_shared& audioFrame)
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

AudioBuffer::shared PCMADecoder::GetDecodedAudioFrame()
{
	uint8_t* in = audioFrameInfo.first;
	int inLen = audioFrameInfo.second;

	auto audioBuffer = std::make_shared<AudioBuffer>(inLen, 1);
	for (int j = 0; j< inLen;j++)
	{
		if(audioBuffer->AddPCM(alaw2linear(*in), j))
			in++;
		else	
			return {};
	}
	return audioBuffer;
}
