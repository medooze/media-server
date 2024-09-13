#include "TestCommon.h"
#include "AudioPipe.h"
#include <future>
struct AudioPipeParam
{
    int sampleRate; 
    int numChannels;
    int frameSize;
};

void helperTestPCMData(const AudioPipeParam& playParam, const AudioPipeParam& recParam, int numPlayBuffers)
{
    int playSampleRate = playParam.sampleRate, playFrameSize = playParam.frameSize;
    int recSampleRate = recParam.sampleRate, recFrameSize = recParam.frameSize;
    int numChannels = playParam.numChannels;

    int numPlaySamples = numPlayBuffers*playFrameSize + recSampleRate/50;
    int numRecSamples = numPlaySamples * (recSampleRate/(float)(playSampleRate));
    int numRecAudioBuffers = numRecSamples / recFrameSize - 1;
    numPlayBuffers = numPlaySamples / playFrameSize;
    AudioPipe audPipe(playSampleRate);

    int16_t in[numPlaySamples * numChannels];
    int16_t* inLoc = in;
    // allocate big enough
    int16_t resampled[numPlaySamples * numChannels * 2];
    int16_t* resampledLoc = resampled;
    int16_t out[numRecSamples * numChannels];
    int16_t* outLoc = out;
    int totalResampled=0;
    int resampledSamples=0;

    for(int i=0;i<numPlaySamples;i++) 
        for(int ch=0;ch<numChannels;ch++)
            in[i*numChannels+ch]=(i+1)%(1<<15);

    audPipe.StartRecording(recSampleRate);
    auto fut = std::async(std::launch::async, [&] {
        for(int i=0;i<numPlayBuffers+1;i++)
        {
            auto audioBuffer = std::make_shared<AudioBuffer>(playFrameSize, numChannels);
            audioBuffer->SetSamples(inLoc, playFrameSize);
		    audPipe.PlayBuffer(audioBuffer);
            resampledSamples = audioBuffer->GetNumSamples()*numChannels;
            memcpy(resampledLoc, audioBuffer->GetData(), resampledSamples*sizeof(SWORD));
            inLoc += playFrameSize*numChannels;
            resampledLoc += resampledSamples;
            totalResampled += resampledSamples;
        };
    });
    std::vector<int> audioEncoderPTS;
    audPipe.StartPlaying(playSampleRate, numChannels);
	for(int i=0; i< numRecAudioBuffers; ++i)
    {
        auto recAudioBuffer = audPipe.RecBuffer(recFrameSize);
        if(recAudioBuffer) 
        {
            SWORD* pcm = (SWORD*)(recAudioBuffer->GetData());
            int totalSamples = recAudioBuffer->GetNumSamples()*numChannels;
            memcpy(outLoc, pcm, totalSamples*sizeof(SWORD));
            outLoc += totalSamples;
            audioEncoderPTS.push_back(recAudioBuffer->GetTimestamp());
        }
    }; 
    for (int i = 0;i < numRecAudioBuffers * recFrameSize;i++)
        EXPECT_EQ(resampled[i], out[i]) << "audio data differ at index " << i;
}

TEST(TestAudioPipe, audioBufferPCM)
{
    int playSampleRate = 48000, recSampleRate = 48000;
    int playFrameSizes[] = {1024};
    int recFrameSizes[] = {960, 1920};

    int numChannels = 1;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;

    for(int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for(int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperTestPCMData(playParam, recParam, numAudioBuffers);
        }
    }
}