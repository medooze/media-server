#include "TestCommon.h"
#include "Buffer.h"
#include "BufferWritter.h"
#include "h264/H26xNal.h"


TEST(TestH26xNal, NalSliceAnnexB)
{
	//Nal units for testing <startCodeSize,nalSize>
	const std::vector<std::pair<size_t, size_t>> nals = {
		{3, 1},
		{4, 2},
		{3, 3},
	};

	Buffer annnexB(200);
	BufferWritter writter(annnexB);
	

	//Create annexb nal stream	
	for (auto [starCodeSize, nalSize] : nals)
	{
		for (int i=0;i<starCodeSize-1; i++)
			writter.Set1(0x00);
		writter.Set1(0x01);
		for (int i=0;i<nalSize; i++)
			writter.Set1(0xdd);
	}
	annnexB.SetSize(writter.GetLength());

	int nalCount = 0;
	BufferReader reader(annnexB);
	NalSliceAnnexB(reader, [&](BufferReader& nalReader) {
		ASSERT_LT(nalCount, nals.size());
		EXPECT_EQ(nalReader.GetLeft(), nals[nalCount].second);
		nalCount++;
	});
	
	EXPECT_EQ(nalCount,nals.size());

}
