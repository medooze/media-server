#include "g711.h"
#include <string.h>
#include "g711codec.h"

#define NUMFRAMES 160

PCMAEncoder::PCMAEncoder(const Properties &properties)
	: audioFrame(std::make_shared<AudioFrame>(AudioCodec::PCMA))
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

AudioFrame::shared PCMAEncoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{
	const SWORD *in = audioBuffer->GetData();
	int inLen = audioBuffer->GetNumSamples();
	if(!in || inLen<=0) return nullptr;
	for (int j = 0; j< inLen ;j++)
	{
		uint8_t alaw = linear2alaw(in[j]);
		audioFrame->AppendMedia(&alaw, 1); 
	}
	audioFrame->SetLength(inLen);
	return audioFrame;
}


int PCMADecoder::Decode(const AudioFrame::const_shared& audioFrame)
{
	//Comprobamos las longitudes
	// inLen: compressed data length in bytes/samples one byte per sample for g711a, outLen: pcm data size in samples
	auto inLen = audioFrame->GetLength();
	uint8_t* in = const_cast<uint8_t*>(audioFrame->GetData());
	if (!in || !inLen)
		return 0;

	auto audioBuffer = std::make_shared<AudioBuffer>(inLen, 1);
	for (int j = 0; j<inLen;j++)
	{ 
		if(audioBuffer->SetSampleAt(j, alaw2linear(*in)))
			in++;
		else	
			return 0;
	}
	audioBufferQueue.push(std::move(audioBuffer));
	return 1;	
}

AudioBuffer::shared PCMADecoder::GetDecodedAudioFrame()
{
	if(audioBufferQueue.empty())
		return {};

	auto audioBuffer = audioBufferQueue.front();
	audioBufferQueue.pop();
	return audioBuffer;	
}