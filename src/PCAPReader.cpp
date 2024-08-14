#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <time.h>
#include "tools.h"
#include "PCAPReader.h"

const size_t   PCAP_HEADER_SIZE = 24;
const size_t   PCAP_PACKET_HEADER_SIZE = 16;
const uint32_t PCAP_MAGIC_COOKIE = 0xa1b2c3d4;

PCAPReader::PCAPReader()
{
}

PCAPReader::~PCAPReader()
{
	Close();
}

bool PCAPReader::Open(const char* file)
{
	Log(">PCAPReader::Open() | Opening pcap file [%s]\n",file);
	// Open filename
	if ((fd = open(file, O_RDONLY))==-1)
		return Error("-PCAPReader::Open() | Error opening pcap file\n");

	//Read the header
	if (read(fd,data,PCAP_HEADER_SIZE)!=PCAP_HEADER_SIZE)
		return Error("-PCAPReader::Open() | Error reading magic cookie from pcap file\n");

	uint32_t cookie = get4(data,0);

	if (cookie != PCAP_MAGIC_COOKIE)
	{
		//Try reverse order
		cookie = get4Reversed(data, 0);

		if (cookie != PCAP_MAGIC_COOKIE)
			return Error("-PCAPReader::Open() | PCAP magic cookie %x nof found, got %x.\n", PCAP_MAGIC_COOKIE, cookie);
		
		reversed = true;
	}
		
	Log("<PCAPReader::Open() | Opened pcap file [reversed:%d]\n", reversed);
	return true;
}

void PCAPReader::Rewind()
{
	//Go just after header
	(void)lseek(fd, PCAP_HEADER_SIZE, SEEK_SET);
	
	//Debug
	UltraDebug("-PCAPReader::Rewind() | retry at [pos:%ld]\n",lseek(fd, 0, SEEK_CUR));
}

uint64_t PCAPReader::Next()
{
retry:
	UltraDebug("-PCAPReader::GetNextPacket() | retry at [pos:%d]\n",lseek(fd, 0, SEEK_CUR));

	//read header
	if (read(fd,data,PCAP_PACKET_HEADER_SIZE)!=PCAP_PACKET_HEADER_SIZE)
		//Error
		return false;
	
	//Get packet data
	uint32_t seconds	= !reversed ? get4(data, 0)	: get4Reversed(data, 0);
	uint32_t nanoseonds	= !reversed ? get4(data, 4)	: get4Reversed(data, 4);
	uint32_t size		= !reversed ? get4(data, 8)	: get4Reversed(data, 8);
	uint32_t captured	= !reversed ? get4(data, 12)	: get4Reversed(data, 12);
	
	//Get current timestamp
	uint64_t ts = (((uint64_t)seconds)*1000000+nanoseonds);

	UltraDebug("-Got packet captured:%u size:%d\n",captured,size);
	//Read the packet
// Ignore coverity error: Passing tainted expression "captured" to "read", which uses it as an offset.
// coverity[tainted_data]
	if (read(fd,data,captured)!=size) 
	{
		Error("-PCAPReader::GetNextPacket() | Short read\n");
		//retry
		goto retry;
	}
	//Get outmost protocl
	uint32_t protocol	= !reversed ? get2(data, 0) : get2Reversed(data, 0);

	//linux cooked capture or ethenet
	uint32_t ini = protocol == 0x0008 ? 20 : 14;

	// Get the udp size including udp headers
	uint16_t udpLen = get2(data, ini + 24);

	//Check length
	if (udpLen!=(size - ini - 20) || udpLen<8)
	{
		Error("-PCAPReader::GetNextPacket() | Wrong UDP packet len:%u[%x] size:%d\n",udpLen, udpLen, size);
		//retry
		goto retry;
	}
	//Get ip and ports
	originIp	= !reversed ? get4(data, ini + 12) : get4Reversed(data, ini + 12);
	destIp		= !reversed ? get4(data, ini + 16) : get4Reversed(data, ini + 16);
	originPort	= !reversed ? get2(data, ini + 20) : get2Reversed(data, ini + 20);
	destPort	= !reversed ? get2(data, ini + 24) : get2Reversed(data, ini + 24);
	
	//The udp packet
	packet	  = data + ini + 28;
	packetLen = udpLen - 8;
	
	UltraDebug("-PCAPReader::GetNextPacket() | got packet [len:%d,pos:%d,ts:%llu]\n",packetLen,lseek(fd, 0, SEEK_CUR),ts);
	
	//Return timestamp of this packet
	return ts;
}

uint64_t PCAPReader::Seek(const uint64_t time)
{
	//Go to the beginning
	Rewind();
	
	while(true)
	{
		//Get current position
		auto pos = lseek(fd, 0, SEEK_CUR);
	
		//read header
		if (read(fd,data,PCAP_PACKET_HEADER_SIZE)!=PCAP_PACKET_HEADER_SIZE)
			//Error
			break;

		//Get packet data
		uint32_t seconds	= !reversed ? get4(data, 0) : get4Reversed(data, 0);
		uint32_t nanoseonds	= !reversed ? get4(data, 4) : get4Reversed(data, 4);
		uint32_t packetSize	= !reversed ? get4(data, 8) : get4Reversed(data, 8);
		[[maybe_unused]] uint32_t packetLen = !reversed ? get4(data, 12) : get4Reversed(data, 12);
		uint64_t ts = (((uint64_t)seconds)*1000000+nanoseonds);

		//If we have got to the correct time
		if (ts>=time)
		{
			//Go to the begining of the packet
// Ignore Coverity error: "pos" is passed to a parameter that cannot be negative.
// coverity[negative_returns]
			(void)lseek(fd, pos, SEEK_SET);
			//Return packet time
			return ts;
		}
	
		//Skip packet data
		if (!lseek(fd, packetSize, SEEK_CUR))
			//Error
			break;
	}
	
	//Go to the beginning
	Rewind();
	
	//Error
	return 0;
}

bool PCAPReader::Close()
{
	Log("-PCAPReader::Close()\n");

	if (fd!=-1)
		//Close pcapc file
		close(fd);
	
	//Closed
	fd = -1;
	
	//Done
	return true;
}
