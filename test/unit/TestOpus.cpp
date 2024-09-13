#include "TestCommon.h"
#include "AudioEncoderWorker.h"
#include "AudioPipe.h"
#include "./helper/TestAudioEncodingHelper.h"


TEST(TestOpusCodec, OpusEncodeSameSampleRate)
{
	AudioCodec::Type codecType = AudioCodec::GetCodecForName("OPUS");
	Properties props;
	props.SetProperty("opus.application.audio", true);

	// params for generating raw pcm data
	int numAudioFrames = 99, decoderFrameSize = 1024, inputSampleRate=48000, inputNumChannels=1;
	float frequency = 1000.0, amplitude = 0.5;
	int startPTS = 0;
	// opus encoder params
	int sampleRate = 48000, numChannels = 1, opusFrameSize = sampleRate * 20 / 1000;

	AudioPCMParams inputParams = {
		numAudioFrames,
		decoderFrameSize,
		inputSampleRate,
		inputNumChannels,
		startPTS,
		frequency,
		amplitude};
	
	TestAudioEncoder(codecType, props, sampleRate, numChannels, opusFrameSize, inputParams);
}

TEST(TestOpusCodec, OpusEncodeUpsamplingAACFrameSize)
{

	AudioCodec::Type codecType = AudioCodec::GetCodecForName("OPUS");
	Properties props;
	props.SetProperty("opus.application.audio", true);

	// params for generating raw pcm data
	int numAudioFrames = 97, decoderFrameSize = 1024, inputSampleRate=44100, inputNumChannels=1;
	float frequency = 1000.0, amplitude = 0.5;
	int startPTS = 0;
	// opus encoder params
	int sampleRate = 48000, numChannels = 1, opusFrameSize = sampleRate * 20 / 1000;

	AudioPCMParams inputParams = {
		numAudioFrames,
		decoderFrameSize,
		inputSampleRate,
		inputNumChannels,
		startPTS,
		frequency,
		amplitude};

	TestAudioEncoder(codecType, props, sampleRate, numChannels, opusFrameSize, inputParams);
}

TEST(TestOpusCodec, OpusEncodeUpsamplingRandomFrameSize)
{
	AudioCodec::Type codecType = AudioCodec::GetCodecForName("OPUS");
	Properties props;
	props.SetProperty("opus.application.audio", true);

	// params for generating raw pcm data
	int numAudioFrames = 9, decoderFrameSize = 851, inputSampleRate=44100, inputNumChannels=1;
	float frequency = 1000.0, amplitude = 0.5;
	int startPTS = 0;
	// opus encoder params
	int sampleRate = 48000, numChannels = 1, opusFrameSize = sampleRate * 20 / 1000;

	AudioPCMParams inputParams = {
		numAudioFrames,
		decoderFrameSize,
		inputSampleRate,
		inputNumChannels,
		startPTS,
		frequency,
		amplitude};

	TestAudioEncoder(codecType, props, sampleRate, numChannels, opusFrameSize, inputParams);
}