#include "TestCommon.h"
#include "audiodecoder.h"
#include "AudioPipe.h"
#include "./helper/audioAVPacketGenerator.h"

static constexpr int AAC_FRAME_SIZE = 1024;

TEST(TestAAC, AACDecode)
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
        uint64_t frameTime = 10;
        int numDecodedSamples = 0;
        const uint8_t config[5] = {0x11, 0x90, 0x56, 0xe5, 0x00};
        AudioCodec::Type type = AudioCodec::GetCodecForName("AAC");

        // generate encoded packets to feed to audioDecoder
        std::queue<AVPacket*> inputPackets = avGenerator.GenerateAVPackets(amplitude);
        int numPackets = inputPackets.size();

        AudioPipe audPipe(sampleRate);
	    AudioDecoderWorker worker;

        worker.AddAudioOuput(&audPipe);
        worker.SetAACConfig(config, sizeof(config)/sizeof(config[0]));
		worker.Start();

        audPipe.StartRecording(sampleRate);
        for (int i = 0; i < numPackets; ++i)
        {
            auto packet = inputPackets.front();
            auto frame = std::make_unique<AudioFrame>(type);
            frame->SetNumChannels(numChannels);
            frame->SetClockRate(params.sampleRate);
            frame->AppendMedia(packet->data, packet->size);
            worker.onMediaFrame(*frame);
            int len = audPipe.RecBuffer(outLoc, AAC_FRAME_SIZE, frameTime);
            outLoc+=AAC_FRAME_SIZE*numChannels;
            numDecodedSamples+=len;
            av_packet_free(&packet);
            inputPackets.pop();
        }

        ASSERT_EQ(numDecodedSamples, (numAACFrames+1)*AAC_FRAME_SIZE) << "decoded samples and expected num decoded samples are of unequal length";
        double channel[numDecodedSamples];
        for(int ch = 0; ch < numChannels; ++ch)
        {
            for(int i=0;i<numDecodedSamples;++i)
                channel[i] = (double)(outBuf[i*numChannels + ch]/32768.0);
            
            std::pair<double,double> res = findPeakFrequency(channel, sampleRate, numDecodedSamples);
            EXPECT_NEAR(res.first, freqInHz, 10) << "unexpected difference in frequency between generated sine tone and decoded sine tone: expected %f - " << freqInHz << "actual %f" << res.first; 
            EXPECT_NEAR(res.second, amplitude, amplitude*0.1) << "unexpected difference in amplitude between generated sine tone and decoded sine tone: expected %f - " << amplitude << "actual %f" << res.second; 
        }
    }
}