#include "TestCommon.h"
#include "audiodecoder.h"
#include "AudioPipe.h"
#include "./helper/audioAVPacketGenerator.h"
#include <future>

static constexpr int AAC_FRAME_SIZE = 1024;

TEST(TestAACCodec, AACDecode)
{
	uint64_t channelLayout = AV_CH_LAYOUT_STEREO;
	int numAACFrames = 50;
	int sampleRate = 48000, numChannels = av_get_channel_layout_nb_channels(channelLayout), numSamples = AAC_FRAME_SIZE*numAACFrames;
	int64_t bitrate = 192000;
	float freqInHz = 1000.0, amplitude = 0.5;
	AVSampleFormat sampleFmt = AV_SAMPLE_FMT_FLTP;

	AudioEncodingParams params = {
		sampleRate,
		numChannels,
		bitrate,
		sampleFmt,
		channelLayout,
		AV_CODEC_ID_AAC};
	AudioAVPacketGenerator avGenerator(params, numSamples, freqInHz, sampleRate);

	if(avGenerator.IsValidSampleFormat(sampleFmt))
	{
		// add 1 for AAC-LC latency
		SWORD outBuf[(numAACFrames+1)*AAC_FRAME_SIZE*numChannels];
		SWORD* outLoc = outBuf;
		int numDecodedSamples = 0;
		const uint8_t config[5] = {0x11, 0x90, 0x56, 0xe5, 0x00};
		AudioCodec::Type type = AudioCodec::GetCodecForName("AAC");

		// generate encoded packets to feed to audioDecoder
		std::queue<AVPacket*> inputPackets = avGenerator.GenerateAVPackets(amplitude);
		// create a dummy packet to avoid deadlock situation, used to signal pthread cond variable in audioPipe.PlayBuffer() called in decoding process
		AVPacket *dummy = av_packet_clone(inputPackets.front());
		int numPackets = inputPackets.size();
		
		AudioPipe audPipe(sampleRate);
		AudioDecoderWorker worker;

		worker.AddAudioOuput(&audPipe);
		worker.SetAACConfig(config, sizeof(config)/sizeof(config[0]));
		worker.Start();

		audPipe.StartRecording(sampleRate);
		// pushing audio frame through worker.onMediaFrame() has run asynchronously, 
		// otherwise deadlock would occur due to indefinite wait pthread cond variable in audio pipe
		auto fut = std::async(std::launch::async, [&] {
			for(int i=0;i<numPackets;i++)
			{
				auto packet = inputPackets.front();
				auto frame = std::make_unique<AudioFrame>(type);
				frame->SetNumChannels(numChannels);
				frame->SetClockRate(params.sampleRate);
				frame->AppendMedia(packet->data, packet->size);
				worker.onMediaFrame(*frame);
				av_packet_free(&packet);
				inputPackets.pop();
			};

			auto frame = std::make_unique<AudioFrame>(type);
			frame->AppendMedia(dummy->data, dummy->size);
			worker.onMediaFrame(*frame);
			
		});
		for (int i = 0; i < numPackets; ++i)
		{
			int len = audPipe.RecBuffer(outLoc, AAC_FRAME_SIZE);
			outLoc+=AAC_FRAME_SIZE*numChannels;
			numDecodedSamples+=len;
		}

		ASSERT_EQ(numDecodedSamples, (numAACFrames+1)*AAC_FRAME_SIZE) << "decoded samples and expected num decoded samples are of unequal length";
		double channel[numDecodedSamples];
		for(int ch = 0; ch < numChannels; ++ch)
		{
			for(int i=0;i<numDecodedSamples;++i)
				channel[i] = (double)(outBuf[i*numChannels + ch]/32768.0);
			
			std::pair<double,double> res = findPeakFrequency(channel, sampleRate, numDecodedSamples);
			EXPECT_NEAR(res.first, freqInHz, 10) << "unexpected difference in frequency between generated sine tone and decoded sine tone: expected = " << freqInHz << ", actual = " << res.first; 
			EXPECT_NEAR(res.second, amplitude, amplitude*0.1) << "unexpected difference in amplitude between generated sine tone and decoded sine tone: expected = " << amplitude << ", actual = " << res.second; 
		}
	}
}