#include "mpegts.h"
#include "mpegtsenc.h"
#include "psi.h"
#include "BufferWritter.h"
#include "bitstream.h"
#include "crc32c/crc32c.h"

using namespace mpegts;

Buffer CreatePAT(uint16_t programNumber, uint16_t pmtPid, uint8_t continutyCounter)
{
	Buffer buffer(188);
	BufferWritter writer(buffer);
	
	Header tsHeader;
	tsHeader.syncByte = 0x47;
	tsHeader.transportErrorIndication = 0;
	tsHeader.payloadUnitStartIndication = true;
	tsHeader.transportPriority = false;
	tsHeader.packetIdentifier = psi::ProgramAssociation::PID;
	tsHeader.transportScramblingControl = 0;
	tsHeader.adaptationFieldControl = AdaptationFieldControl::PayloadOnly;
	tsHeader.continuityCounter = continutyCounter;
	
	tsHeader.Encode(writer);
	
	writer.Set1(0); // payload pointer
	
	psi::Table table;
	table.tableId = psi::ProgramAssociation::TABLE_ID;
	table.sectionSyntaxIndicator = true;
	table.privateBit = 0;
	table._reserved1 = 0x3;
	table.sectionLength = 5 + 4 + 4; // Only one program
	
	table.Encode(writer);
	
	psi::SyntaxData syntax;
	syntax.tableIdExtension = 0x0;
	syntax.isCurrent = 1;
	syntax.sectionNumber = 0x0;
	syntax.versionNumber = 0;
	syntax._reserved1 = 0x3;
	syntax.lastSectionNumber = 0x0;
	
	syntax.Encode(writer);
	
	psi::ProgramAssociation pa;
	pa.programNum = programNumber;
	pa.pmtPid = pmtPid;
	
	pa.Encode(writer);
	
	uint32_t crc32 = crc32c::Crc32c(buffer.GetData() + 5, writer.GetLength() - 5);
	writer.Set4(crc32);
	
	writer.PadTo(mpegts::Packet::PacketSize, 0xff);
	
	buffer.SetSize(writer.GetLength());
	return buffer;
}

Buffer CreatePMT(uint16_t pid, std::map<uint16_t, uint16_t> streamPidMap, uint8_t continutyCounter)
{
	Buffer buffer;
	BufferWritter writer(buffer);
	
	Header tsHeader;
	tsHeader.syncByte = 0x47;
	tsHeader.transportErrorIndication = 0;
	tsHeader.payloadUnitStartIndication = true;
	tsHeader.transportPriority = false;
	tsHeader.packetIdentifier = pid;
	tsHeader.transportScramblingControl = 0;
	tsHeader.adaptationFieldControl = AdaptationFieldControl::PayloadOnly;
	tsHeader.continuityCounter = continutyCounter;
	tsHeader.Encode(writer);
	
	writer.Set1(0); // payload pointer
	
	psi::Table table;
	table.tableId = psi::ProgramMap::TABLE_ID;
	table.privateBit = 0;
	table.sectionSyntaxIndicator = true;
	table.sectionLength = 5 + 2 + 2 + streamPidMap.size() * 5;
	table._reserved1 = 0x3;
	
	psi::SyntaxData sdata;
	sdata.tableIdExtension = 0x1; // program number
	sdata.isCurrent = true;
	sdata.versionNumber = 0;
	sdata._reserved1 = 0x3;
	sdata.sectionNumber = 0x0;
	sdata.lastSectionNumber = 0x0;
	tsHeader.Encode(writer);

	BitWritter bitwriter(writer, 4);
	bitwriter.Put(3, 0x7);	// Reserved
	bitwriter.Put(13, 0x1FFF);	// PCR_PID, no PCR
	bitwriter.Put(4, 0xf);	// reserved
	bitwriter.Put(12, 0);	// program info length
	
	psi::ProgramMap pm;	
	for (auto& [streamType, pid] : streamPidMap) 
	{
		psi::ProgramMap::ElementaryStream info;
		info.streamType = streamType;
		info.pid = pid;
		
		pm.streams.push_back(info);
	}
	
	pm.Encode(writer);
	
	uint32_t crc32 = crc32c::Crc32c(buffer.GetData() + 5, writer.GetLength() - 5);
	writer.Set4(crc32);
	
	writer.PadTo(mpegts::Packet::PacketSize, 0xff);
	
	buffer.SetSize(writer.GetLength());
	
	return buffer;
}

std::vector<Buffer> CreatePES(uint16_t pid, const uint8_t* media, size_t size, uint8_t& continutyCounter)
{
	std::vector<Buffer> packets;
	
	bool first = true;
	
	Header tsHeader;
	tsHeader.syncByte = 0x47;
	tsHeader.transportErrorIndication = 0;
	tsHeader.payloadUnitStartIndication = true;
	tsHeader.transportPriority = false;
	tsHeader.packetIdentifier = pid;
	tsHeader.transportScramblingControl = 0;
	tsHeader.adaptationFieldControl = AdaptationFieldControl::PayloadOnly;
	tsHeader.continuityCounter = continutyCounter;
	
	pes::Header pesHeader;
	pesHeader.packetStartCodePrefix = 0x000001;
	pesHeader.streamId = 0xe0; // video
	
	return packets;
}
