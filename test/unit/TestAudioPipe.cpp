#include "TestCommon.h"
#include "AudioPipe.h"
#include <future>
struct AudioPipeParam
{
	int sampleRate; 
	int numChannels;
	int frameSize;
};
// this helper function test pts
void helperTestAudioPipe(const AudioPipeParam& playParam, const AudioPipeParam& recParam, const std::vector<int>& playPTS, const std::vector<int>& expectedPTS, std::queue<std::pair<int,int>>& recJumpInfo, bool jump=false)
{
    int playSampleRate = playParam.sampleRate, playFrameSize = playParam.frameSize;
    int recSampleRate = recParam.sampleRate, recFrameSize = recParam.frameSize;
    int numChannels = playParam.numChannels;

    int numPlayBuffers = playPTS.size();
    int numPlaySamples = numPlayBuffers*playFrameSize;
    int numRecSamples = numPlaySamples * (recSampleRate/(float)(playSampleRate));
    int numRecAudioBuffers = numRecSamples / recFrameSize;

    std::vector<int> actualRecPTS;
    int16_t in[numPlaySamples * numChannels];
    int16_t* inLoc = in;
    for (int i=0;i<numPlaySamples;i++) 
        for (int ch=0;ch<numChannels;ch++)
            in[i*numChannels+ch]=(i+1)%(1<<15);
    
    AudioPipe audPipe(playSampleRate);

    audPipe.StartRecording(recSampleRate);
    auto fut = std::async(std::launch::async, [&] {
        for (int i=0;i<numPlayBuffers;i++)
        {
            auto audioBuffer = std::make_shared<AudioBuffer>(playFrameSize, numChannels);
            audioBuffer->SetSamples(inLoc, playFrameSize);
            audioBuffer->SetTimestamp(playPTS[i]);
            audPipe.PlayBuffer(audioBuffer);
            inLoc += playFrameSize*numChannels;
        };
    });
    
    audPipe.StartPlaying(playSampleRate, numChannels);
    for (int i=0; i< numRecAudioBuffers; ++i)
    {
        auto recAudioBuffer = audPipe.RecBuffer(recFrameSize);
        if (recAudioBuffer) 
            actualRecPTS.push_back(recAudioBuffer->GetTimestamp());
    }; 
    fut.wait();
    for (int i = 0; i < std::min(actualRecPTS.size(), expectedPTS.size()); ++i)
    {
        auto ptsDiff = abs(actualRecPTS[i]-expectedPTS[i]);
        EXPECT_TRUE(ptsDiff <= 3) << "pts diff: " << ptsDiff;
    }
}
// this helper function creates decoder pts
void helperCreatePlayPTS(int initPTS, int playFrameSize, std::vector<int>& playPTS, std::queue<std::pair<int,int>> jumpInfo, int maxPTS=0)
{
    int pts = initPTS;
    for (int i=0;i<playPTS.size();i++)
    {
        playPTS[i] = pts;
        pts += playFrameSize;
        if (maxPTS!=0) pts%=maxPTS;
    }
    while (!jumpInfo.empty())
    {
        auto item = jumpInfo.front();
        playPTS[item.first] += item.second;
        jumpInfo.pop();
        std::pair<int,int> next;
        if (!jumpInfo.empty()) 
        {
            auto next = jumpInfo.front();

            for (int i=item.first+1;i<next.first;i++)
                playPTS[i] = playPTS[i-1]+playFrameSize;
        }
        else 
        {
            for (int i=item.first+1;i<playPTS.size();i++)
                playPTS[i] = playPTS[i-1] + playFrameSize;
        }
    }
}
// this helper function creates expected encoder pts
std::vector<int> helperCreateRecPTS(const AudioPipeParam& playParam, const AudioPipeParam& recParam, const std::vector<int>& playPTS, std::queue<std::pair<int,int>>& jumpInfo, std::queue<std::pair<int,int>>& skip,int maxPTS=0)
{
    int playSampleRate = playParam.sampleRate, playFrameSize = playParam.frameSize;
    int recSampleRate = recParam.sampleRate, recFrameSize = recParam.frameSize;
    
    std::vector<int> resampledPTS;
    std::vector<int> recPTS;
   
    playFrameSize = std::round<int>(playFrameSize * (recSampleRate / (double)playSampleRate));
    resampledPTS.push_back(playPTS[0]);
    int pts = resampledPTS[0];
    int jumpIdx = 0, lastJumpIdx=0;
    bool jump = !jumpInfo.empty();
    for (int i = 1; i < playPTS.size();i++)
    {
        int tmp = std::round<QWORD>((playPTS[i]-playPTS[0]) * (recSampleRate / (double)playSampleRate)) + playPTS[0];
        if (maxPTS!=0) tmp%=maxPTS;
        resampledPTS.push_back(tmp);
    }

    while (!jumpInfo.empty())
    {
        jumpIdx = jumpInfo.front().first;
        int maxPTS = resampledPTS[jumpIdx-1]+playFrameSize;
        int diff;
        while (pts < maxPTS)
        {
            diff = maxPTS - pts;
            recPTS.push_back(pts);
            pts += recFrameSize;
        }
        skip.push({recPTS.size(), jumpIdx});
        pts = resampledPTS[jumpIdx];
        jumpInfo.pop();
    }
    pts = resampledPTS[jumpIdx];
    for (int i=jumpIdx;i<resampledPTS.size();i++)
    {
        recPTS.push_back(pts);
        pts+=recFrameSize;
        if (maxPTS!=0) pts%=maxPTS;
    }
    lastJumpIdx = jumpIdx;
    while (lastJumpIdx<resampledPTS.size() && pts < resampledPTS[lastJumpIdx]+playFrameSize)
    {
        recPTS.push_back(pts);
        pts+=recFrameSize;
        if (maxPTS!=0) pts%=maxPTS;
        lastJumpIdx++;
    }
    return recPTS;
}

TEST(TestAudioPipe, stereoGoodPTS)
{
    int playSampleRate = 48000, recSampleRate = 48000;
    int playFrameSizes[] = {1024};
    int recFrameSizes[] = {960};

    int numChannels = 2;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo; 
    std::queue<std::pair<int,int>> recJumpInfo; 
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, goodPTS)
{
    int playSampleRate = 48000, recSampleRate = 48000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo; 
    std::queue<std::pair<int,int>> recJumpInfo; 
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, goodPTSIntegerUpsampling)
{
    int playSampleRate = 24000, recSampleRate = 48000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;

    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, goodPTSIntegerDownsampling)
{
    int playSampleRate = 48000, recSampleRate = 24000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;

    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, goodPTSFractionalUpsampling)
{
    int playSampleRate = 44100, recSampleRate = 48000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;

    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, goodPTSFractionalDownsampling)
{
    int playSampleRate = 48000, recSampleRate = 44100;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;

    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, ptsJump)
{
    int playSampleRate = 48000, recSampleRate = 48000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};
    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    int maxPTS = 0;
    bool jump = true;
    std::queue<std::pair<int,int>> jumpInfo;  
      
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        jumpInfo.push({2,  2.5*playFrameSizes[i]});
        jumpInfo.push({7,  4*playFrameSizes[i]});
        jumpInfo.push({8,  5.3*playFrameSizes[i]});
        jumpInfo.push({23, 12*playFrameSizes[i]});
        std::queue<std::pair<int,int>> recJumpInfo;
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {    
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, recJumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, recJumpInfo, true);
        }
    }
}

TEST(TestAudioPipe, ptsJumpIntegerUpsampling)
{
    int playSampleRate = 24000, recSampleRate = 48000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};
    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    int maxPTS = 0;
    bool jump = true;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        jumpInfo.push({2,  2.5*playFrameSizes[i]});
        jumpInfo.push({7,  4*playFrameSizes[i]});
        jumpInfo.push({8,  5.3*playFrameSizes[i]});
        jumpInfo.push({23, 12*playFrameSizes[i]});
        std::queue<std::pair<int,int>> recJumpInfo;
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {    
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, recJumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, recJumpInfo, true);
        }
    }
}

TEST(TestAudioPipe, ptsJumpFractionalUpsampling)
{
    int playSampleRate = 44100, recSampleRate = 48000;
    int playFrameSizes[] = {1024};
    int recFrameSizes[] = {960, 1920};
    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    int maxPTS = 0;
    bool jump = true;
   
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        jumpInfo.push({2,  2.5*playFrameSizes[i]});
        jumpInfo.push({7,  4*playFrameSizes[i]});
        jumpInfo.push({8,  5.3*playFrameSizes[i]});
        jumpInfo.push({23, 12*playFrameSizes[i]});
        std::queue<std::pair<int,int>> recJumpInfo;
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {    
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, recJumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, recJumpInfo, true);
        }
    }
}

TEST(TestAudioPipe, ptsJumpIntegerDownsampling)
{
    int playSampleRate = 48000, recSampleRate = 24000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};
    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    int maxPTS = 0;
    bool jump = true;
   
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        jumpInfo.push({2,  2.5*playFrameSizes[i]});
        jumpInfo.push({7,  4*playFrameSizes[i]});
        jumpInfo.push({8,  5.3*playFrameSizes[i]});
        jumpInfo.push({23, 12*playFrameSizes[i]});
        std::queue<std::pair<int,int>> recJumpInfo;
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {    
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, recJumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, recJumpInfo, true);
        }
    }
}

TEST(TestAudioPipe, ptsJumpFractionalDownsampling)
{
    int playSampleRate = 48000, recSampleRate = 44100;
    int playFrameSizes[] = {1024};
    int recFrameSizes[] = {960, 1920};
    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    int maxPTS = 0;
    bool jump = true;
   
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        jumpInfo.push({2,  2.5*playFrameSizes[i]});
        jumpInfo.push({7,  4*playFrameSizes[i]});
        jumpInfo.push({8,  5.3*playFrameSizes[i]});
        jumpInfo.push({23, 12*playFrameSizes[i]});
        std::queue<std::pair<int,int>> recJumpInfo;
        for(int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {    
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo);
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, recJumpInfo);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, recJumpInfo, true);
        }
    }
}

TEST(TestAudioPipe, ptsWraparound)
{
    int playSampleRate = 48000, recSampleRate = 48000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            int maxPTS = playFrameSizes[i] * 16;
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo, maxPTS);
            maxPTS = std::round<int>(maxPTS*(recSampleRate/(double)playSampleRate));
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo, maxPTS);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, ptsWraparoundIntegerUpsampling)
{
    int playSampleRate = 24000, recSampleRate = 48000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            int maxPTS = playFrameSizes[i] * 16;
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo, maxPTS);
            maxPTS = std::round<int>(maxPTS*(recSampleRate/(double)playSampleRate));
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo, maxPTS);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, ptsWraparoundFractionalUpsampling)
{
    int playSampleRate = 44100, recSampleRate = 48000;
    int playFrameSizes[] = {1024};
    int recFrameSizes[] = {960, 1920};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            int maxPTS = playFrameSizes[i] * 16;
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo, maxPTS);
            maxPTS = std::round<int>(maxPTS*(recSampleRate/(double)playSampleRate));
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo, maxPTS);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, ptsWraparoundIntegerDownsampling)
{
    int playSampleRate = 48000, recSampleRate = 24000;
    int playFrameSizes[] = {30, 100, 1024};
    int recFrameSizes[] = {50, 100, 960};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            int maxPTS = playFrameSizes[i] * 16;
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo, maxPTS);
            maxPTS = std::round<int>(maxPTS*(recSampleRate/(double)playSampleRate));
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo, maxPTS);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}

TEST(TestAudioPipe, ptsWraparoundFractionalDownsampling)
{
    int playSampleRate = 48000, recSampleRate = 44100;
    int playFrameSizes[] = {1024};
    int recFrameSizes[] = {960, 1920};

    int numChannels = 1;
    int initPTS = 21;
    int numAudioBuffers = 37;
    AudioPipeParam playParam;
    AudioPipeParam recParam;
    std::vector<int> playPTS(numAudioBuffers, 0);
    std::vector<int> recPTS;
    std::queue<std::pair<int,int>> jumpInfo;  
    for (int i=0; i < sizeof(playFrameSizes)/sizeof(playFrameSizes[0]);i++)
    {
        playParam = {playSampleRate, numChannels, playFrameSizes[i]};
        for (int j=0; j < sizeof(recFrameSizes)/sizeof(recFrameSizes[0]);j++)
        {
            int maxPTS = playFrameSizes[i] * 16;
            recParam = {recSampleRate, numChannels, recFrameSizes[j]};                              
            helperCreatePlayPTS(initPTS, playFrameSizes[i], playPTS, jumpInfo, maxPTS);
            maxPTS = std::round<int>(maxPTS*(recSampleRate/(double)playSampleRate));
            recPTS = helperCreateRecPTS(playParam, recParam, playPTS, jumpInfo, jumpInfo, maxPTS);
            helperTestAudioPipe(playParam, recParam, playPTS, recPTS, jumpInfo, false);
        }
    }
}