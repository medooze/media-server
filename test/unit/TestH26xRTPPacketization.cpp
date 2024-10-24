#include "TestCommon.h"
#include "h264/H264Packetizer.h"
#include "h265/H265Packetizer.h"

class H264PacketizerForTest : public H264Packetizer
{
    public:
        void TestRTPFUA(VideoFrame& frame, BufferReader nal)
        {
            H264Packetizer::EmitNal(frame, nal);
        }
};

class H265PacketizerForTest : public H265Packetizer
{
    public:
        void TestRTPFUA(VideoFrame& frame, BufferReader nal)
        {
            H265Packetizer::EmitNal(frame, nal);
        }
};


TEST(TestFUA, h264Fragmentation)
{
    static constexpr int nalPayloadSize = 1201;
    static constexpr int H264NalHeaderSize = 1;
    static constexpr int FUPayloadHdrSize = 1;
    static constexpr int FUHeaderSize = 1;

	uint8_t nalData[H264NalHeaderSize + nalPayloadSize] = {0};
    nalData[0] = 0x65;

    H264PacketizerForTest h264Packetizer;
    BufferReader nal(nalData, H264NalHeaderSize + nalPayloadSize);
    VideoFrame frame(VideoCodec::H264);
    h264Packetizer.TestRTPFUA(frame, nal);
   
    int expectedPos = 4 + H264NalHeaderSize;
    int expectedRTPPacekts = nalPayloadSize / (RTPPAYLOADSIZE - FUPayloadHdrSize - FUHeaderSize) + 1;
	auto packetLen = nalPayloadSize / expectedRTPPacekts;
	int mod = nalPayloadSize % expectedRTPPacekts;

    auto& rtpInfo = frame.GetRtpPacketizationInfo();
   	EXPECT_EQ(expectedRTPPacekts, rtpInfo.size());
    for (int i=0; i < expectedRTPPacekts; i++)
    {
        auto len = packetLen + (mod>0 ? 1:0);
        uint8_t expectedFU[FUHeaderSize+FUPayloadHdrSize] = {(nalData[0] & 0b0'11'00000) | 28, 0x05};

        if (i==0)
            // set Start bit
            expectedFU[FUHeaderSize+FUPayloadHdrSize-1] |= 0b10'000000;
        if (i==expectedRTPPacekts-1)
            //  set End bit
            expectedFU[FUHeaderSize+FUPayloadHdrSize-1] |= 0b01'000000;
        EXPECT_EQ(rtpInfo[i].GetPos(), expectedPos);
        EXPECT_EQ(rtpInfo[i].GetSize(), len);
        EXPECT_EQ(rtpInfo[i].GetPrefixLen(), 2);
        EXPECT_EQ(memcmp(rtpInfo[i].GetPrefixData(),  expectedFU, 2), 0);
       
        expectedPos += len;
        mod--;
    }
}

TEST(TestFUA, h265Fragmentation)
{
    static constexpr int FUPayloadHdrSize = 2;
    static constexpr int FUHeaderSize = 1;

    static constexpr int nalPayloadSize = 19427;
	uint8_t nalData[HEVCParams::RTP_NAL_HEADER_SIZE + nalPayloadSize] = {0};
    nalData[0] = 0x26;
    nalData[1] = 0x01;
    uint16_t naluHeader = nalData[0] << 8 | nalData[1];
    BYTE nalUnitType = (naluHeader >> 9) & 0b111111;
	BYTE nalLID =  (naluHeader >> 3) & 0b111111;
	BYTE nalTID = naluHeader & 0b111;

    H265PacketizerForTest h265Packetizer;
    BufferReader nal(nalData, HEVCParams::RTP_NAL_HEADER_SIZE + nalPayloadSize);
    VideoFrame frame(VideoCodec::H265);
    h265Packetizer.TestRTPFUA(frame, nal);

    int expectedRTPPacekts = nalPayloadSize / (RTPPAYLOADSIZE - FUPayloadHdrSize - FUHeaderSize) + 1;
    auto packetLen = nalPayloadSize / expectedRTPPacekts;
	int mod = nalPayloadSize % expectedRTPPacekts;
    int expectedPos = 4 + HEVCParams::RTP_NAL_HEADER_SIZE;

    const uint16_t nalHeaderFU = ((uint16_t)(HEVC_RTP_NALU_Type::UNSPEC49_FU) << 9)
		| ((uint16_t)(nalLID) << 3)
		| ((uint16_t)(nalTID));

    auto& rtpInfo = frame.GetRtpPacketizationInfo();
   	EXPECT_EQ(expectedRTPPacekts, rtpInfo.size());
    for (int i=0; i < expectedRTPPacekts; i++)
    {
        auto len = packetLen + (mod>0 ? 1:0);
        uint8_t expectedFU[FUHeaderSize+FUPayloadHdrSize] = {(nalHeaderFU & 0xff00) >> 8, nalHeaderFU & 0xff,  0x13};

        if (i==0)
            // set Start bit
            expectedFU[FUHeaderSize+FUPayloadHdrSize-1] |= 0b10'000000;
        if (i==expectedRTPPacekts-1)
            //  set End bit
            expectedFU[FUHeaderSize+FUPayloadHdrSize-1] |= 0b01'000000;
        EXPECT_EQ(rtpInfo[i].GetPos(), expectedPos);
        EXPECT_EQ(rtpInfo[i].GetSize(), len);
        EXPECT_EQ(rtpInfo[i].GetPrefixLen(), FUHeaderSize+FUPayloadHdrSize);
        EXPECT_EQ(memcmp(rtpInfo[i].GetPrefixData(),  expectedFU, FUHeaderSize+FUPayloadHdrSize), 0);
        expectedPos += len;
        mod--;
    }
}