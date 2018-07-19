#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <time.h>
#include "PCAPReader.h"

const size_t   PCAP_HEADER_SIZE = 24;
const size_t   PCAP_PACKET_HEADER_SIZE = 16;
const uint32_t PCAP_MAGIC_COOKIE = 0xa1b2c3d4;

PCAPReader::PCAPReader()
{
}

bool PCAPReader::Open(const char* file)
{
	Log("-Opening pcap file [%s]\n",file);
	// Open filename
	if ((fd = open(file, O_RDONLY))==-1)
		return Error("Error opening pcap file\n");

	//Read the header
	if (read(fd,data,PCAP_HEADER_SIZE)!=PCAP_HEADER_SIZE)
		return Error("Error reading magic cookie from pcap file\n");

	uint32_t cookie = get4(data,0);

	if (cookie!=PCAP_MAGIC_COOKIE)
		return Error("PCAP magic cookie %x nof founr, got %x, reversed are not supported (yet).\n",PCAP_MAGIC_COOKIE,cookie);

	return true;
}

void PCAPReader::Rewind()
{
	//Go just after header
	lseek(fd, PCAP_HEADER_SIZE, SEEK_SET);
	
	//Debug
	UltraDebug("-PCAPReader::Rewind() | retry at [pos:%d]\n",lseek(fd, 0, SEEK_CUR));
}

RTPPacket::shared PCAPReader::GetNextPacket(const RTPMap& rtpMap,const RTPMap& extMap)
{
retry:
	//UltraDebug("-PCAPReader::GetNextPacket() | retry at [pos:%d]\n",lseek(fd, 0, SEEK_CUR));

	//read header
	if (read(fd,data,PCAP_PACKET_HEADER_SIZE)!=PCAP_PACKET_HEADER_SIZE)
		//Error
		return nullptr;
	
	//Get packet data
	uint32_t seconds	= get4(data,0);
	uint32_t nanoseonds	= get4(data,4);
	uint32_t packetSize	= get4(data,8);
	uint32_t packetLen	= get4(data,12);
	QWORD ts = (((QWORD)seconds)*1000000+nanoseonds);

	//Debug("-Got packet size:%u len:%d\n",packetSize,packetLen);
	//Read the packet
	if (read(fd,data,packetSize)!=packetSize) 
	{
		Error("-PCAPReader::GetNextPacket() | Short read\n");
		//retry
		goto retry;
	}

	// Get the udp size including udp headers
	uint16_t udpLen = get2(data,38);

	if (packetSize!=packetLen || udpLen!=(packetSize-34) || udpLen<8)
	{
		Error("-PCAPReader::GetNextPacket() | Wrong UDP packet len:%u\n",udpLen);
		//retry
		goto retry;
	}
	//The RTP packet
	uint8_t* buffer = data + 42;
	uint32_t bufferLen = udpLen - 8;
	
	//Check it is not RTCP
	if (RTCPCompoundPacket::IsRTCP(buffer,bufferLen))
	{
		//Debug
		//UltraDebug("-PCAPReader::GetNextPacket() | skipping rtcp\n");
		//Ignore this try again
		goto retry;
	}
	
	RTPHeader header;
	RTPHeaderExtension extension;

	//Parse RTP header
	uint32_t ini = header.Parse(buffer,bufferLen);
	
	//On error
	if (!ini)
	{
		//Debug
		Error("-PCAPReader::GetNextPacket() | Could not parse RTP header ini=%u len=%d udp=%u packet=%u\n",ini,bufferLen-ini,udpLen,packetSize);
		//Dump it
		Dump(buffer+ini,bufferLen-ini);
		//Ignore this try again
		goto retry;
	}
	
	//If it has extension
	if (header.extension)
	{
		//Parse extension
		int l = extension.Parse(extMap,buffer+ini,bufferLen-ini);
		//If not parsed
		if (!l)
		{
			///Debug
			Error("-PCAPReader::GetNextPacket() | Could not parse RTP header extension ini=%u len=%d udp=%u packet=%u\n",ini,bufferLen-ini,udpLen,packetSize);
			//Dump it
			Dump(buffer+ini,bufferLen-ini);
			//retry
			goto retry;
		}
		//Inc ini
		ini += l;
	}
	
	//Check size with padding
	if (header.padding)
	{
		//Get last 2 bytes
		WORD padding = get1(buffer,bufferLen-1);
		//Ensure we have enought size
		if (bufferLen-ini<padding)
		{
			///Debug
			Debug("-PCAPReader::GetNextPacket() | RTP padding is bigger than size [padding:%u,size%u]\n",padding,len);
			//Ignore this try again
			goto retry;
		}
		//Remove from size
		bufferLen -= padding;
	}

	//Check we have payload
	if (ini>=bufferLen)
	{
		///Debug
		UltraDebug("-PCAPReader::GetNextPacket() | Refusing to create a packet with empty payload [size:%u,ini:%u,len:%u]\n",size,len,ini);
		//Ignore this try again
		goto retry;
	}
	

	//Get initial codec
	BYTE codec = rtpMap.GetCodecForType(header.payloadType);
	
	//Check codec
	if (codec==RTPMap::NotFound)
	{
		//Error
		Error("-PCAPReader::GetNextPacket() | RTP packet type unknown [%d]\n",header.payloadType);
		//retry
		goto retry;
	}
	
	//Get media
	MediaFrame::Type media = GetMediaForCodec(codec);
	
	//Create normal packet
	auto packet = std::make_shared<RTPPacket>(media,codec,header,extension);

	//Set the payload
	packet->SetPayload(buffer+ini,bufferLen-ini);

	//Set capture time in ms
	packet->SetTime(ts/1000);
	
	return packet;
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
		uint32_t seconds	= get4(data,0);
		uint32_t nanoseonds	= get4(data,4);
		uint32_t packetSize	= get4(data,8);
		uint32_t packetLen	= get4(data,12);
		QWORD ts = (((QWORD)seconds)*1000000+nanoseonds);

		//If we have got to the correct time
		if (ts>=time)
		{
			//Go to the begining of the packet
			lseek(fd, pos, SEEK_SET);
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
	
	//Close pcapc file
	close(fd);
	
	//Done
	return true;
}
