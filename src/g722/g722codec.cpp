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
	: audioFrame(std::make_shared<AudioFrame>(AudioCodec::G722))
{
	///Set type
	type = AudioCodec::G722;
	// 20 ms @ 16 Khz = 320 samples
	numFrameSamples = 20 * 16;
	//Init encoder
	g722_encode_init(&encoder,64000, 2);
	audioFrame->DisableSharedBuffer();
}

AudioFrame::shared G722Encoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{
	const SWORD *in = audioBuffer->GetData();
	int inLen = audioBuffer->GetNumSamples() * audioBuffer->GetNumChannels();
	if(!in || inLen<=0) return nullptr;
	int len = g722_encode(&encoder,audioFrame->GetData(), in, inLen);
	audioFrame->SetLength(len);
	return audioFrame;
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

int G722Decoder::Decode(const AudioFrame::const_shared& audioFrame)
{
	auto inLen = audioFrame->GetLength();
	uint8_t* in =  const_cast<uint8_t*>(audioFrame->GetData());
	if(!in || !inLen) 
		return 0;

	auto audioBuffer = std::make_shared<AudioBuffer>(inLen, 1);

	g722_decode(&decoder, (SWORD*)audioBuffer->GetData(), in, inLen);
	audioBufferQueue.push(std::move(audioBuffer));
	return 1;
}

AudioBuffer::shared G722Decoder::GetDecodedAudioFrame()
{
	if(audioBufferQueue.empty())
		return {};

	auto audioBuffer = audioBufferQueue.front();
	audioBufferQueue.pop();
	return audioBuffer;
}