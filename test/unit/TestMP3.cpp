#include "TestCommon.h"
#include "AudioDecoderWorker.h"
#include "AudioPipe.h"
#include "./helper/TestAudioDecodingHelper.h"

static constexpr int MP3_FRAME_SIZE = 1152;


TEST(TestMP3Codec, MP3Decode)
{
	uint64_t channelLayout = AV_CH_LAYOUT_STEREO;
	int numMP3Frames = 50;
	int sampleRate = 48000, numChannels = av_get_channel_layout_nb_channels(channelLayout), numSamples = MP3_FRAME_SIZE*numMP3Frames;
	int64_t bitrate = 96000;
	float freqInHz = 1000.0, amplitude = 0.5;
	AVSampleFormat sampleFmt = AV_SAMPLE_FMT_FLTP;

	AudioEncodingParams params = {
		sampleRate,
		numChannels,
		bitrate,
		sampleFmt,
		channelLayout,
		AV_CODEC_ID_MP3};
	AudioPacketGenerator avGenerator(params, numSamples, freqInHz, sampleRate);

	if(avGenerator.IsValidSampleFormat(sampleFmt))
	{
		SWORD outBuf[(numMP3Frames)*MP3_FRAME_SIZE*numChannels];
		SWORD* outLoc = outBuf;
		int numDecodedSamples = 0;
        const uint8_t config[4] = {0x11, 0xce, 0x85, 0xf7};
		AudioCodec::Type type = AudioCodec::GetCodecForName("MP3");
		// generate encoded packets to feed to audioDecoder
		std::queue<AVPacket*> inputPackets = avGenerator.GenerateAVPackets(amplitude);
		// create a dummy packet to avoid deadlock situation, used to signal pthread cond variable in audioPipe.PlayBuffer() called in decoding process
		AVPacket *dummy = av_packet_clone(inputPackets.front());
		int numPackets = inputPackets.size();
		
		AudioPipe audPipe(sampleRate);
		AudioDecoderWorker worker;

		worker.AddAudioOuput(&audPipe);
		worker.Start();

		audPipe.StartRecording(sampleRate);
		// pushing audio frame through worker.onMediaFrame() has run asynchronously, 
		// otherwise deadlock would occur due to indefinite wait pthread cond variable in audio pipe
		auto fut = std::async(std::launch::async, [&] {
			int pts = 0;
			for(int i=0;i<numPackets;i++)
			{
				auto packet = inputPackets.front();
				auto frame = std::make_unique<AudioFrame>(type);
				frame->SetTimestamp(pts);
				frame->SetNumChannels(numChannels);
				frame->SetClockRate(params.sampleRate);
                frame->SetCodecConfig(config, sizeof(config)/sizeof(config[0]));
				frame->AppendMedia(packet->data, packet->size);
				worker.onMediaFrame(*frame);
				pts += MP3_FRAME_SIZE;
				av_packet_free(&packet);
				inputPackets.pop();
			};
			auto frame = std::make_unique<AudioFrame>(type);
			frame->AppendMedia(dummy->data, dummy->size);
			worker.onMediaFrame(*frame);
		});

		for (int i = 0; i < numPackets-1; ++i)
		{
			auto audioBuffer = audPipe.RecBuffer(MP3_FRAME_SIZE);
			memcpy(outLoc, (SWORD*)audioBuffer->GetData(), MP3_FRAME_SIZE*sizeof(int16_t)*numChannels);
			outLoc+=MP3_FRAME_SIZE*numChannels;
			numDecodedSamples+=(audioBuffer->GetNumSamples()/audioBuffer->GetNumChannels());
		}

		ASSERT_EQ(numDecodedSamples, (numMP3Frames)*MP3_FRAME_SIZE) << "decoded samples and expected num decoded samples are of unequal length";
		float channel[numDecodedSamples];
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