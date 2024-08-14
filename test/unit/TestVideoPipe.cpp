#include "TestCommon.h"
#include "VideoPipe.h"

struct ScalingParam
{
    int srcVideoWidth;
    int srcVideoHeight;
    int scaleResolutionToHeight;
    float scaleResolutionDownBy;
    VideoPipe::AllowedDownScaling allowedDownScaling;
    std::pair<int, int> srcPicPAR;
};

VideoBuffer::const_shared testPictureResize(const ScalingParam &param)
{
    // randomly picked video fps since this test has nothing to do with fps
    int fps = 25;
    uint64_t frameTime = 1E6 / fps;

    VideoPipe vidPipe;
    float scaleResolutionDownBy = param.scaleResolutionDownBy;
    int scaleResolutionToHeight = param.scaleResolutionToHeight;
    auto allowedDownScaling = param.allowedDownScaling;

    vidPipe.Init(scaleResolutionDownBy, scaleResolutionToHeight, allowedDownScaling);

    VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(param.srcVideoWidth, param.srcVideoHeight);
    sharedVidBuffer->SetPixelAspectRatio({param.srcPicPAR.first, param.srcPicPAR.second});
    // enqueue the shared pointer of the video buffer
    size_t qsize = vidPipe.NextFrame(sharedVidBuffer);
    // consume the video buffer enqueued at above step
    auto pic = vidPipe.GrabFrame(frameTime / 1000);
    return pic;
}

TEST(TestVideoPipe, pictureResizeScaleToHeight)
{
    {
        // Input Params 1 : DAR = 4:3, NonSquarePAR with PAR Numerator < PAR Denominator, allowedDownScaling == Any
        int width = 704, height = 480;
        int scaleResolutionToHeight = 360;
        int parNum = 10, parDen = 11;
        std::pair<int, int> expectedResolution(480, scaleResolutionToHeight);
        ScalingParam param = {
            width,
            height,
            scaleResolutionToHeight,
            0.0f,
            VideoPipe::Any,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic->GetWidth(), expectedResolution.first);
        ASSERT_EQ(pic->GetHeight(), expectedResolution.second);
    }
    {
        // Input Params 2 : DAR = 16:9, NonSquarePAR with PAR Numerator > PAR Denominator, allowedDownScaling == Any
        int width = 704, height = 480;
        int scaleResolutionToHeight = 360;
        int parNum = 40, parDen = 33;
        std::pair<int, int> expectedResolution(640, scaleResolutionToHeight);

        ScalingParam param = {
            width,
            height,
            scaleResolutionToHeight,
            0.0f,
            VideoPipe::Any,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic->GetWidth(), expectedResolution.first);
        ASSERT_EQ(pic->GetHeight(), expectedResolution.second);
    }
    {
        // Input Params 3 : DAR = 16:9, SquarePAR, allowedDownScaling == Any
        int width = 3840, height = 2160;
        int parNum = 1, parDen = 1;
        std::vector<std::pair<int, int>> expectedResolutions;
        expectedResolutions.push_back({2560, 1440});
        expectedResolutions.push_back({1920, 1080});
        expectedResolutions.push_back({1280, 720});
        expectedResolutions.push_back({854, 480});
        expectedResolutions.push_back({640, 360});
        expectedResolutions.push_back({426, 240});

        for (uint8_t i = 0; i < expectedResolutions.size(); i++)
        {
            ScalingParam param = {
                width,
                height,
                expectedResolutions[i].second,
                0.0f,
                VideoPipe::Any,
                {parNum, parDen}};
            auto pic = testPictureResize(param);

            ASSERT_EQ(pic->GetWidth(), expectedResolutions[i].first);
            ASSERT_EQ(pic->GetHeight(), expectedResolutions[i].second);
        }
    }
    {
        // Input Params 4 : allowedDownScaling == Any, scale up
        int width = 704, height = 480;
        int scaleResolutionToHeight = 720;
        int parNum = 40, parDen = 33;
        std::pair<int, int> expectedResolution(1280, scaleResolutionToHeight);

        ScalingParam param = {
            width,
            height,
            scaleResolutionToHeight,
            0.0f,
            VideoPipe::Any,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic->GetWidth(), expectedResolution.first);
        ASSERT_EQ(pic->GetHeight(), expectedResolution.second);
    }
    {
        // Input Params 5 : allowedDownScaling == SameOrLower
        int width = 704, height = 480;
        int scaleResolutionToHeight = 720;
        int parNum = 10, parDen = 11;

        ScalingParam param = {
            width,
            height,
            scaleResolutionToHeight,
            0.0f,
            VideoPipe::SameOrLower,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic, nullptr);
    }
    {
        // Input Params 6 :  allowedDownScaling == LowerOnly
        int width = 704, height = 480;
        int scaleResolutionToHeight = 480;
        int parNum = 10, parDen = 11;

        ScalingParam param = {
            width,
            height,
            scaleResolutionToHeight,
            0.0F,
            VideoPipe::LowerOnly,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic, nullptr);
    }
}

TEST(TestVideoPipe, pictureResizeScaleDownBy)
{
    {
        // Input Params 1 : DAR = 4:3, NonSquarePAR with PAR Numerator < PAR Denominator, allowedDownScaling == Any
        int width = 704, height = 480;
        float scaleResolutionDownBy = 1.3333;
        int parNum = 10, parDen = 11;
        std::pair<int, int> expectedResolution(480, 360);

        ScalingParam param = {
            width,
            height,
            0,
            scaleResolutionDownBy,
            VideoPipe::Any,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic->GetWidth(), expectedResolution.first);
        ASSERT_EQ(pic->GetHeight(), expectedResolution.second);
    }
    {
        // Input Params 2 : DAR = 16:9, NonSquarePAR with PAR Numerator > PAR Denominator, allowedDownScaling == Any
        int width = 704, height = 480;
        float scaleResolutionDownBy = 1.3333;
        int parNum = 40, parDen = 33;
        std::pair<int, int> expectedResolution(640, 360);

        ScalingParam param = {
            width,
            height,
            0,
            scaleResolutionDownBy,
            VideoPipe::Any,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic->GetWidth(), expectedResolution.first);
        ASSERT_EQ(pic->GetHeight(), expectedResolution.second);
    }
    {
        // Input Params 3 : DAR = 16:9, SquarePAR, allowedDownScaling == Any
        int width = 3840, height = 2160;
        int parNum = 1, parDen = 1;
        std::vector<std::pair<int, int>> expectedResolutions;

        expectedResolutions.push_back({2560, 1440});
        expectedResolutions.push_back({1920, 1080});
        expectedResolutions.push_back({1280, 720});
        expectedResolutions.push_back({854, 480});
        expectedResolutions.push_back({640, 360});
        expectedResolutions.push_back({426, 240});
        std::vector<float> scaleDownBy(expectedResolutions.size(), 0.0f);

        for (uint8_t i = 0; i < expectedResolutions.size(); i++)
        {
            scaleDownBy[i] = float(height) / expectedResolutions[i].second;
            ScalingParam param = {
                width,
                height,
                0,
                scaleDownBy[i],
                VideoPipe::Any,
                {parNum, parDen}};
            auto pic = testPictureResize(param);

            ASSERT_EQ(pic->GetWidth(), expectedResolutions[i].first);
            ASSERT_EQ(pic->GetHeight(), expectedResolutions[i].second);
        }
    }
    {
        // Input Params 4 : allowedDownScaling == Any, scale up
        int width = 704, height = 480;
        float scaleResolutionDownBy = 0.6666;
        int parNum = 40, parDen = 33;
        std::pair<int, int> expectedResolution(1280, 720);

        ScalingParam param = {
            width,
            height,
            0,
            scaleResolutionDownBy,
            VideoPipe::Any,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic->GetWidth(), expectedResolution.first);
        ASSERT_EQ(pic->GetHeight(), expectedResolution.second);
    }
    {
        // Input Params 5 : allowedDownScaling == SameOrLower
        int width = 704, height = 480;
        float scaleResolutionDownBy = 0.8f;
        int parNum = 10, parDen = 11;

        ScalingParam param = {
            width,
            height,
            0,
            scaleResolutionDownBy,
            VideoPipe::SameOrLower,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic, nullptr);
    }
    {
        // Input Params 6 :  allowedDownScaling == LowerOnly
        int width = 704, height = 480;
        float scaleResolutionDownBy = 1.0f;
        int parNum = 10, parDen = 11;

        ScalingParam param = {
            width,
            height,
            0,
            scaleResolutionDownBy,
            VideoPipe::LowerOnly,
            {parNum, parDen}};
        auto pic = testPictureResize(param);

        ASSERT_EQ(pic, nullptr);
    }
}

TEST(TestVideoPipe, matchingfps)
{
    int infps = 60;
    int outfps = 60;
    int clockrate = 90000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    for (int i = 0; i < 30; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    for (int i = 0; i < 30; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), (int)(i * (double)clockrate / outfps));
    }
}

TEST(TestVideoPipe, downrating)
{
    int infps = 60;
    int outfps = 30;
    int clockrate = 90000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    for (int i = 0; i < 30; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    for (int i = 0; i < 15; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), (int)(i * (double)clockrate / outfps));
    }
}

TEST(TestVideoPipe, uprating)
{
    int infps = 30;
    int outfps = 60;
    int clockrate = 90000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    for (int i = 0; i < 30; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    for (int i = 0; i < 30; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), (int)(i * (double)clockrate / infps));
    }
}

TEST(TestVideoPipe, downratingNotExact)
{
    int infps = 60;
    int outfps = 30;
    int clockrate = 1000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    for (int i = 0; i < 30; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    for (int i = 0; i < 15; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), (int)(i * (double)clockrate / outfps));
    }
}

TEST(TestVideoPipe, upratingNotExact)
{
    int infps = 30;
    int outfps = 60;
    int clockrate = 1000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    for (int i = 0; i < 30; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    for (int i = 0; i < 30; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), (int)(i * (double)clockrate / infps));
    }
}



TEST(TestVideoPipe, maxDelay)
{
    int infps = 50;
    int outfps = 50;
    int clockrate = 1000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    // 3 frames
    int totalFrames = 30;
    int delayInFrames = 3;
    vidPipe.SetMaxDelay(delayInFrames * clockrate / infps);

    for (int i = 0; i < totalFrames; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_LE(queued, 3);
    }

    for (int i = 0; i < delayInFrames; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), (int)((totalFrames - delayInFrames + i) * (double)clockrate / outfps));
    }
}

TEST(TestVideoPipe, cancelledGrab)
{
    int outfps = 60;
    int clockrate = 90000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    auto blocking_grab_frame = std::async(std::launch::async, [&]() -> auto
                                          { return vidPipe.GrabFrame(0); });

    vidPipe.CancelGrabFrame();

    auto pic = blocking_grab_frame.get();
    ASSERT_EQ(pic, nullptr);
}

TEST(TestVideoPipe, downratingCancelGrabInvalidFrame)
{
    int infps = 60;
    int outfps = 30;
    int clockrate = 90000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    // Enqueue a few frames. First is consumed and second is dropped for downrating
    for (int i = 0; i < 2; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), 0U);
    }

    // Start grab frame that is waiting for the next input
    // without cancel will block forever
    auto blocking_grab_frame = std::async(std::launch::async, [&]() -> auto
                                          { return vidPipe.GrabFrame(0); });

    // Force the condition to be waiting on another frame OR a cancel.
    // I.e. It would have woken up and handled second frame which it
    // decides to drop before we call cancel
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2s);
    vidPipe.CancelGrabFrame();

    auto pic = blocking_grab_frame.get();
    ASSERT_EQ(pic, nullptr);
}

TEST(TestVideoPipe, dontBlockOnEmptyQueueWhenDownrating)
{
    int infps = 60;
    int outfps = 30;
    int clockrate = 90000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    // Enqueue a few frames. First is consumed and second is dropped for downrating
    for (int i = 0; i < 2; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), 0U);
    }

    // Start grab frame that is waiting for the next input
    // for at most 10msec, however with bug will wait forever
    auto blocking_grab_frame = std::async(std::launch::async, [&]() -> auto
                                          { return vidPipe.GrabFrame(10); });

    auto pic = blocking_grab_frame.get();
    ASSERT_EQ(pic, nullptr);
}


TEST(TestVideoPipe, setMaxDelayWithExistingDataThenOverflows)
{
    // Mostly was written to test the SetMaxDelay works when there is already data in the queue.
    // However following that I also verify the new max delay takes effect correctly
    // by pushing frames past the new delay and expecting them to be dropped

    // Change fps/clock rate values to something a bit easier to debug
    int infps = 20;
    int outfps = 10;
    int clockrate = 1000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);

    
    // Lets fill up the pipe
    uint64_t nextPushTimestamp = 0;
    uint64_t nextPullTimestamp = 0;
    for (int i = 0; i < VideoPipe::MaxOutstandingFramesDefault; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(nextPushTimestamp);
        nextPushTimestamp += (double)clockrate / infps;
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 1);
    }

    // Now lets change the max latency to a few frames larger than the default queue size
    // so that it forces a circular queue resize
    // We do this while there is data in the queue already
    float f = (VideoPipe::MaxOutstandingFramesDefault + 2.0) / infps * 1000.0;
    uint32_t maxDelayMS = std::round(f);
    vidPipe.SetMaxDelay(maxDelayMS);

    // Now lets drain the queue and make sure the items are still correct in there
    for (int i = 0; i < VideoPipe::MaxOutstandingFramesDefault/2; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), nextPullTimestamp);
        nextPullTimestamp += (double)clockrate / outfps;
    }

    // Should be nothing left in queue (well not that is valid for our fps anyway)
    {
        auto pic = vidPipe.GrabFrame(10);
        ASSERT_EQ(pic, nullptr);
    }

    // Lets set the max delay to a value that requires the queue size to be adjusted
    int fillDelayFrames = maxDelayMS * infps / 1000;
    for (int i = 0; i < fillDelayFrames; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(nextPushTimestamp);
        nextPushTimestamp += (double)clockrate / infps;
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
    }

    // Lets add a few extra frames to go over the max latency
    const size_t extraFrames = 10;
    for (int i = fillDelayFrames; i < fillDelayFrames + extraFrames; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(nextPushTimestamp);
        nextPushTimestamp += (double)clockrate / infps;
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
    }

    // We would have dropped extraFrames frames from input
    nextPullTimestamp += extraFrames * ((double)clockrate / infps);

    for (int i = 0; i < fillDelayFrames/2; ++i)
    {
        auto pic = vidPipe.GrabFrame(10);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), nextPullTimestamp);
        nextPullTimestamp += (double)clockrate / outfps;
    }

}

TEST(TestVideoPipe, timestampJumps)
{
    int infps = 50;
    int outfps = 50;
    int clockrate = 1000;

    int width = 640;
    int height = 480;

    VideoPipe vidPipe;
    vidPipe.Init();
    vidPipe.StartVideoCapture(width, height, outfps);


    //First  frame with a bad TS
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(86000);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, 1);
    }

    // Enqueue a few frames with normal TS
    for (int i = 0; i < 2; ++i)
    {
        VideoBuffer::shared sharedVidBuffer = std::make_shared<VideoBuffer>(width, height);
        sharedVidBuffer->SetClockRate(clockrate);
        sharedVidBuffer->SetTimestamp(i * (double)clockrate / infps);
        auto queued = vidPipe.NextFrame(sharedVidBuffer);
        ASSERT_EQ(queued, i + 2);
    }

    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), 86000);
    }

    for (int i = 0; i < 2; ++i)
    {
        auto pic = vidPipe.GrabFrame(0);
        ASSERT_NE(pic, nullptr);
        ASSERT_EQ(pic->GetTimestamp(), (int)(i * (double)clockrate / outfps));
    }
}