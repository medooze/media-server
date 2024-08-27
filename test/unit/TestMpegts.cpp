#include "TestCommon.h"
#include "mpegts/mpegts.h"

TEST(TestMpegts, TestTsHead)
{
	mpegts::Header header;
	
	header.adaptationFieldControl = mpegts::AdaptationFieldControl::AdaptationFieldOnly;
	header.syncByte = 0xff;
	header.continuityCounter = 9;
	header.packetIdentifier = 1;
	header.payloadUnitStartIndication = false;
	header.transportErrorIndication = true;
	header.transportPriority = true;
	header.transportScramblingControl = 0x3;
	
	Buffer data(4);
	BufferWritter writer(data);
	header.Encode(writer);
	ASSERT_EQ(4, writer.GetLength());
	
	data.SetSize(writer.GetLength());
	
	BufferReader reader(data);
	auto parsed = mpegts::Header::Parse(reader);
	
	ASSERT_EQ(header.syncByte, parsed.syncByte);
	ASSERT_EQ(header.adaptationFieldControl, parsed.adaptationFieldControl);
	ASSERT_EQ(header.continuityCounter, parsed.continuityCounter);
	ASSERT_EQ(header.packetIdentifier, parsed.packetIdentifier);
	ASSERT_EQ(header.payloadUnitStartIndication, parsed.payloadUnitStartIndication);
	ASSERT_EQ(header.transportErrorIndication, parsed.transportErrorIndication);
	ASSERT_EQ(header.transportPriority, parsed.transportPriority);
	ASSERT_EQ(header.transportScramblingControl, parsed.transportScramblingControl);
}

TEST(TestMpegts, TestAdaptationFieldControl)
{
	mpegts::AdaptationField field;
	
	field.adaptationFieldLength = 12;
	field.discontinuityIndicator = false;
	field.randomAccessIndicator = true;
	field.elementaryStreamPriorityIndicator = false;
	field.pcrFlag = true;
	field.opcrFlag = false;
	field.splicingPointFlag = true;
	field.transportPrivateDataFlag = false;
	field.adaptationFieldExtensionFlag = true;
	
	Buffer data(12 + 1);
	BufferWritter writer(data);
	field.Encode(writer);
	ASSERT_EQ(2, writer.GetLength());
	
	data.SetSize(12 + 1);
	
	BufferReader reader(data);
	auto parsed = mpegts::AdaptationField::Parse(reader);
	
	ASSERT_EQ(field.adaptationFieldLength, parsed.adaptationFieldLength);
	ASSERT_EQ(field.discontinuityIndicator, parsed.discontinuityIndicator);
	ASSERT_EQ(field.randomAccessIndicator, parsed.randomAccessIndicator);
	ASSERT_EQ(field.elementaryStreamPriorityIndicator, parsed.elementaryStreamPriorityIndicator);
	ASSERT_EQ(field.pcrFlag, parsed.pcrFlag);
	ASSERT_EQ(field.opcrFlag, parsed.opcrFlag);
	ASSERT_EQ(field.splicingPointFlag, parsed.splicingPointFlag);
	ASSERT_EQ(field.transportPrivateDataFlag, parsed.transportPrivateDataFlag);
	ASSERT_EQ(field.adaptationFieldExtensionFlag, parsed.adaptationFieldExtensionFlag);
}

