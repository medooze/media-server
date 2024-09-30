#include "TestCommon.h"
#include "mpegts/mpegts.h"
#include "mpegts/psi.h"

namespace
{

template <typename T>
std::pair<T, std::unique_ptr<Buffer>> EncodeAndDecode(const T& encodable)
{
	auto temp = std::make_unique<Buffer>(encodable.Size());
	BufferWritter writer(*temp);
	encodable.Encode(writer);
	
	if (encodable.Size() != writer.GetLength())
	{
		throw std::runtime_error(FormatString("Encoded size is not expected: %llu != %llu", encodable.Size(), writer.GetLength()));
	}
	
	temp->SetSize(writer.GetLength());
	
	BufferReader reader(*temp);
	return {T::Parse(reader), std::move(temp)};
}
}

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
	
	auto&& [parsed, coded] = EncodeAndDecode(header);
	
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
	
	field.discontinuityIndicator = false;
	field.randomAccessIndicator = true;
	field.elementaryStreamPriorityIndicator = false;
	field.pcrFlag = true;
	field.opcrFlag = false;
	field.splicingPointFlag = false;
	field.transportPrivateDataFlag = false;
	field.adaptationFieldExtensionFlag = false;
	
	auto&& [parsed, coded] = EncodeAndDecode(field);
	
	ASSERT_EQ(field.discontinuityIndicator, parsed.discontinuityIndicator);
	ASSERT_EQ(field.randomAccessIndicator, parsed.randomAccessIndicator);
	ASSERT_EQ(field.elementaryStreamPriorityIndicator, parsed.elementaryStreamPriorityIndicator);
	ASSERT_EQ(field.pcrFlag, parsed.pcrFlag);
	ASSERT_EQ(field.opcrFlag, parsed.opcrFlag);
	ASSERT_EQ(field.splicingPointFlag, parsed.splicingPointFlag);
	ASSERT_EQ(field.transportPrivateDataFlag, parsed.transportPrivateDataFlag);
	ASSERT_EQ(field.adaptationFieldExtensionFlag, parsed.adaptationFieldExtensionFlag);
}

TEST(TestMpegts, TestCreateStreamId)
{
	ASSERT_EQ(0xca, mpegts::pes::CreateAudioStreamID(10));
	ASSERT_EQ(0xea, mpegts::pes::CreateVideoStreamID(10));
}


TEST(TestMpegts, TestPesHeader)
{
	// Test default value
	mpegts::pes::Header header;
	ASSERT_EQ(0x1, header.packetStartCodePrefix);
	ASSERT_EQ(0, header.streamId);
	ASSERT_EQ(0, header.packetLength);
	ASSERT_EQ(6, header.Size());
	
	// Header size is always same
	header.packetLength = 12;
	ASSERT_EQ(6, header.Size());

	header.streamId = 0xca;
	
	auto [parsed, coded] = EncodeAndDecode(header);
	ASSERT_EQ(header.packetStartCodePrefix, parsed.packetStartCodePrefix);
	ASSERT_EQ(header.streamId, parsed.streamId);
	ASSERT_EQ(header.packetLength, parsed.packetLength);
}


TEST(TestMpegts, TestPesHeaderExtention)
{
	// Test default value
	mpegts::pes::HeaderExtension extension;
	ASSERT_EQ(0x2, extension.markerBits);
	ASSERT_EQ(0, extension.scramblingControl);
	ASSERT_EQ(false, extension.priority);
	ASSERT_EQ(false, extension.dataAlignmentIndicator);
	ASSERT_EQ(false, extension.copyright);
	ASSERT_EQ(false, extension.original);
	ASSERT_EQ(mpegts::pes::PTSDTSIndicator::None, extension.ptsdtsIndicator);
	ASSERT_EQ(false, extension.escrFlag);
	ASSERT_EQ(false, extension.rateFlag);
	ASSERT_EQ(false, extension.trickModeFlag);
	ASSERT_EQ(false, extension.aditionalInfoFlag);
	ASSERT_EQ(false, extension.crcFlag);
	ASSERT_EQ(false, extension.extensionFlag);
	ASSERT_EQ(std::nullopt, extension.pts);
	ASSERT_EQ(std::nullopt, extension.dts);
	
	// Randomly set value
	extension.original = true;
	extension.pts = 3000;
	extension.dts = 2000;
	extension.ptsdtsIndicator = mpegts::pes::PTSDTSIndicator::Both;
	
	auto&& [parsed, coded] = EncodeAndDecode(extension);
	ASSERT_EQ(parsed.markerBits, extension.markerBits);
	ASSERT_EQ(parsed.scramblingControl, extension.scramblingControl);
	ASSERT_EQ(parsed.priority, extension.priority);
	ASSERT_EQ(parsed.dataAlignmentIndicator, extension.dataAlignmentIndicator);
	ASSERT_EQ(parsed.copyright, extension.copyright);
	ASSERT_EQ(parsed.original, extension.original);
	ASSERT_EQ(parsed.ptsdtsIndicator, extension.ptsdtsIndicator);
	ASSERT_EQ(parsed.escrFlag, extension.escrFlag);
	ASSERT_EQ(parsed.rateFlag, extension.rateFlag);
	ASSERT_EQ(parsed.trickModeFlag, extension.trickModeFlag);
	ASSERT_EQ(parsed.aditionalInfoFlag, extension.aditionalInfoFlag);
	ASSERT_EQ(parsed.crcFlag, extension.crcFlag);
	ASSERT_EQ(parsed.extensionFlag, extension.extensionFlag);
	ASSERT_EQ(parsed.pts, extension.pts);
	ASSERT_EQ(parsed.dts, extension.dts);
}


TEST(TestMpegts, TestAdtsHeader)
{
	mpegts::pes::adts::Header header;
	ASSERT_EQ(0xfff, header.syncWord);
	ASSERT_EQ(false, header.version);
	ASSERT_EQ(0, header.layer);
	ASSERT_EQ(false, header.protectionAbsence);
	ASSERT_EQ(0, header.objectTypeMinus1);
	ASSERT_EQ(0, header.samplingFrequency);
	ASSERT_EQ(false, header.priv);
	ASSERT_EQ(0, header.channelConfiguration);
	ASSERT_EQ(false, header.originality);
	ASSERT_EQ(false, header.home);
	ASSERT_EQ(false, header.copyright);
	ASSERT_EQ(false, header.copyrightStart);
	ASSERT_EQ(0, header.frameLength);
	ASSERT_EQ(0, header.bufferFullness);
	ASSERT_EQ(0, header.numberOfFramesMinus1);
	ASSERT_EQ(0, header.crc);
	
	// Randomly set value
	header.objectTypeMinus1 = 1;
	header.samplingFrequency = 2;
	header.originality = true;
	header.crc = 0x2342;
	
	auto&& [parsed, coded] = EncodeAndDecode(header);
	ASSERT_EQ(parsed.syncWord, header.syncWord);
	ASSERT_EQ(parsed.version, header.version);
	ASSERT_EQ(parsed.layer, header.layer);
	ASSERT_EQ(parsed.protectionAbsence, header.protectionAbsence);
	ASSERT_EQ(parsed.objectTypeMinus1, header.objectTypeMinus1);
	ASSERT_EQ(parsed.samplingFrequency, header.samplingFrequency);
	ASSERT_EQ(parsed.priv, header.priv);
	ASSERT_EQ(parsed.channelConfiguration, header.channelConfiguration);
	ASSERT_EQ(parsed.originality, header.originality);
	ASSERT_EQ(parsed.home, header.home);
	ASSERT_EQ(parsed.copyright, header.copyright);
	ASSERT_EQ(parsed.copyrightStart, header.copyrightStart);
	ASSERT_EQ(parsed.frameLength, header.frameLength);
	ASSERT_EQ(parsed.bufferFullness, header.bufferFullness);
	ASSERT_EQ(parsed.numberOfFramesMinus1, header.numberOfFramesMinus1);
	ASSERT_EQ(parsed.crc, header.crc);
}

TEST(TestMpegts, TestProgramAssociation)
{
	mpegts::psi::ProgramAssociation pa;
	pa.programNum = 1;
	pa.pmtPid = 0x20;
	
	mpegts::psi::SyntaxData syntax;
	syntax.tableIdExtension = 0x9;
	syntax.data = pa;
	
	mpegts::psi::Table table;
	table.tableId = 0;
	table.sectionSyntaxIndicator = true;
	table.data = syntax;
	
	auto&& [parsed, coded] = EncodeAndDecode(table);
	ASSERT_EQ(0, parsed.tableId);
	
	auto syntaxParsed = std::get_if<mpegts::psi::SyntaxData>(&parsed.data);
	ASSERT_NE(nullptr, syntaxParsed);
	ASSERT_EQ(syntax.tableIdExtension, syntaxParsed->tableIdExtension);
	
	auto reader = std::get_if<BufferReader>(&syntaxParsed->data);
	ASSERT_NE(nullptr, reader);
	
	auto paParsed = mpegts::psi::ProgramAssociation::Parse(*reader);
	ASSERT_EQ(pa.programNum, paParsed.programNum);
	ASSERT_EQ(pa.pmtPid, paParsed.pmtPid);
}


TEST(TestMpegts, TestProgramMap)
{
	mpegts::psi::ProgramMap pm;
	pm.pcrPid = 0x20;
	
	mpegts::psi::ProgramMap::ElementaryStream es;
	es.streamType = 6;
	es.pid = 0x30;
	pm.streams.push_back(es);
	
	// Create a fake descriptor for testing
	uint8_t descriptor[64];
	for (size_t i = 0;i < 64; i++) descriptor[i] = i;
	
	mpegts::psi::ProgramMap::ElementaryStream es2;
	es2.streamType = 7;
	es2.pid = 0x40;
	es2.descriptor = BufferReader(descriptor, 64);
	pm.streams.push_back(es2);
	
	mpegts::psi::SyntaxData syntax;
	syntax.tableIdExtension = 0x10;
	syntax.data = pm;
	
	mpegts::psi::Table table;
	table.tableId = 0x20;
	table.sectionSyntaxIndicator = true;
	table.data = syntax;
	
	auto&& [parsed, coded] = EncodeAndDecode(table);
	ASSERT_EQ(0x20, parsed.tableId);
		
	// Check original descriptor reader was not changed
	ASSERT_EQ(64, pm.streams[1].descriptor.GetLeft());

	auto syntaxParsed = std::get_if<mpegts::psi::SyntaxData>(&parsed.data);
	ASSERT_NE(nullptr, syntaxParsed);
	ASSERT_EQ(syntax.tableIdExtension, syntaxParsed->tableIdExtension);
	
	auto reader = std::get_if<BufferReader>(&syntaxParsed->data);
	ASSERT_NE(nullptr, reader);
	
	auto pmParsed = mpegts::psi::ProgramMap::Parse(*reader);
	
	ASSERT_EQ(pm.pcrPid, pmParsed.pcrPid);
	ASSERT_EQ(2, pmParsed.streams.size());
	
	ASSERT_EQ(6, pmParsed.streams[0].streamType);
	ASSERT_EQ(0x30, pmParsed.streams[0].pid);
	
	ASSERT_EQ(7, pmParsed.streams[1].streamType);
	ASSERT_EQ(0x40, pmParsed.streams[1].pid);
	ASSERT_EQ(64, pmParsed.streams[1].descriptor.GetLeft());
	ASSERT_EQ(0, memcmp(pmParsed.streams[1].descriptor.PeekData(), descriptor, 64));
}

TEST(TestMpegts, TestTableNoSyntax)
{
	// Create a fake syntax data for testing
	uint8_t tmp[64];
	for (size_t i = 0;i < 64; i++) tmp[i] = i;
	
	mpegts::psi::Table table;
	table.tableId = 0x30;
	table.sectionSyntaxIndicator = false;
	table.data = BufferReader(tmp, 64);
	
	auto&& [parsed, coded] = EncodeAndDecode(table);
	ASSERT_EQ(0x30, parsed.tableId);
	
	auto reader = std::get_if<BufferReader>(&parsed.data);
	ASSERT_NE(nullptr, reader);
	ASSERT_EQ(0, memcmp(reader->PeekData(), tmp, 64));
}
