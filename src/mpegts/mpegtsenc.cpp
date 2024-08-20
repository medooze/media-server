#include "mpegts.h"
#include "mpegtsenc.h"
#include "psi.h"
#include "BufferWritter.h"

using namespace mpegts;

std::vector<Buffer> CreatePAT(uint16_t programNumber, uint16_t pmtPid, uint8_t continutyCounter)
{
	std::vector<Buffer> packets;
	
	Buffer buffer;
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
	
	psi::Table table;
	table.tableId = psi::ProgramAssociation::TABLE_ID;
	table.privateBit = 0;
	table._reserved1 = 0x3;
	
	table.Encode(writer);
	
	psi::ProgramAssociation pa;
	pa.programNum = programNumber;
	pa.pmtPid = pmtPid;
	
	pa.Encode(writer);
	
	packets.push_back(std::move(buffer));
	
	return packets;
}

std::vector<Buffer> CreatePMT(uint16_t pid, std::map<uint16_t, uint16_t> streamPidMap, uint8_t continutyCounter)
{
	std::vector<Buffer> packets;
	
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
	
	psi::Table table;
	table.tableId = psi::ProgramMap::TABLE_ID;
	table.privateBit = 0;
	table._reserved1 = 0x3;
	
	psi::SyntaxData sdata;
	sdata.tableIdExtension = 0x1; // program number
	sdata.isCurrent = true;
	sdata.versionNumber = 0;
	sdata._reserved1 = 0x3;
	sdata.sectionNumber = 0x0;
	sdata.lastSectionNumber = 0x0;

	table.data = sdata;
	
	tsHeader.Encode(writer);

	psi::ProgramMap pm;	
	for (auto it = streamPidMap.begin(); it != streamPidMap.end(); it++) 
	{
		psi::ProgramMap::ElementaryStream info;
		info.streamType = it->first;
		info.pid = it->second;
			
		if (it->first == 0x1B) {
			pm.pcrPid = it->second;
		}
		
		pm.streams.push_back(info);
	}
	
	pm.Encode(writer);
	
	packets.push_back(std::move(buffer));
	
	return packets;
}

std::vector<Buffer> CreatePES(uint16_t pid, const MediaFrame& mediaFrame, uint8_t& continutyCounter)
{
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
	
	
	
}
