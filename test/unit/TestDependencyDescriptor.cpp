#include "TestCommon.h"
#include "rtp.h"
#include "bitstream/BitReader.h"
#include "bitstream/BitWriter.h"

static constexpr int AAC_FRAME_SIZE = 1024;

TEST(TestDependendyDescriptor, BitReader)
{
	BYTE buffer[1];
	/* n=5
	Value	Full binary encoding	ns(n) encoding
	0	000	                00
	1	001	                01
	2	010	                10
	3	011	                110
	4	100	                111
	*/
	std::vector<BYTE> encodings = 
	{	0b00'000000,
		0b01'000000,
		0b10'000000,
		0b110'00000,
		0b111'00000,
	};

	for (int i=0; i< encodings.size(); ++i)
	{
		BitWriter w(buffer, 1);
		BufferReader reader(buffer, 1);
		BitReader r(reader);
		BYTE val;

		w.WriteNonSymmetric(5, i);
		w.Flush();
		val = r.GetNonSymmetric(5);
		EXPECT_EQ(buffer[0], encodings[i]);
		EXPECT_EQ(val, i);
	}
}


TEST(TestDependendyDescriptor, SerializeParser)
{
	BYTE buffer[256];
	BitWriter writter(buffer, sizeof(buffer));

	Logger::EnableDebug(true);
	Logger::EnableUltraDebug(true);

	DependencyDescriptor dd;
	dd.templateDependencyStructure = TemplateDependencyStructure{};
	dd.templateDependencyStructure->dtsCount = 2;
	dd.templateDependencyStructure->chainsCount = 2;
	dd.templateDependencyStructure->frameDependencyTemplates.emplace_back(FrameDependencyTemplate{
		{0, 0},
		{DecodeTargetIndication::Switch, DecodeTargetIndication::Required},
		{1},
		{2,2}
		});
	dd.templateDependencyStructure->decodeTargetProtectedByChain = { 0,0 };
	dd.activeDecodeTargets = { 1,1 };
	dd.templateDependencyStructure->CalculateLayerMapping();

	assert(dd.Serialize(writter));

	auto len = writter.Flush();

	BufferReader bufferReader(buffer,len);
	BitReader reader(bufferReader);

	auto parsed = DependencyDescriptor::Parse(reader);
	ASSERT_TRUE(parsed);

	//Debug
	Dump(buffer, len);
	dd.Dump();
	parsed->Dump();

	EXPECT_EQ(dd,parsed.value());


	writter.Reset();
	DependencyDescriptor dd2;
	dd2.customDecodeTargetIndications = std::vector<DecodeTargetIndication>{ DecodeTargetIndication::Switch, DecodeTargetIndication::Required };
	dd2.customFrameDiffs = std::vector<uint32_t>{ 1 };
	dd2.customFrameDiffsChains = std::vector<uint32_t>{ 2,2 };

	ASSERT_TRUE(dd2.Serialize(writter));

	len = writter.Flush();
	::Dump(buffer, len);


}


TEST(TestDependendyDescriptor, Parser)
{

	BYTE data[10] = { 0x80,   0x00,   0x01,   0x80,   0x00,   0xe2,   0x04,   0xfe, 0x03,   0xbe };
	BufferReader bufferReader(data, 10);
	BitReader reader(bufferReader);
	auto dd = DependencyDescriptor::Parse(reader);

	ASSERT_TRUE(dd->templateDependencyStructure->ContainsFrameDependencyTemplate(0));
	dd->Dump();
}


TEST(TestDependendyDescriptor, ParserScalabilityStructureL2T2KeyShift)
{
	BYTE data[0x23] = { 0xc7   ,0x00   ,0xc6   ,0x80   ,0xe3   ,0x06   ,0x1e   ,0xaa
				,0x82   ,0x80   ,0x40   ,0x28   ,0x28   ,0x05   ,0x14   ,0xd1
				,0x41   ,0x34   ,0x51   ,0x80   ,0x10   ,0xa0   ,0x91   ,0x88
				,0x9a   ,0x09   ,0x40   ,0x3b   ,0xc0   ,0x2c   ,0xc0   ,0x77
				,0xc0   ,0x59   ,0xc0 };
	BufferReader bufferReader(data, sizeof(data));
	BitReader reader(bufferReader);

	auto dd = DependencyDescriptor::Parse(reader);

	dd->Dump();

	BYTE buffer[256];
	BitWriter writter(buffer, sizeof(buffer));

	ASSERT_TRUE(dd->Serialize(writter));

	auto len = writter.Flush();
	::Dump(buffer, len);
	EXPECT_EQ(len, sizeof(data));
	EXPECT_TRUE(memcmp(data, buffer, len) == 0);
}