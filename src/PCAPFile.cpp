#include <sys/stat.h> 
#include <fcntl.h>
#include "PCAPFile.h"
#include "log.h"

const size_t   PCAP_HEADER_SIZE = 24;
const size_t   PCAP_UDP_PACKET_SIZE = 58;
const uint32_t PCAP_MAGIC_COOKIE = 0xa1b2c3d4;

PCAPFile::~PCAPFile() 
{
	//Close jic
	Close();
}

int PCAPFile::Open(const char* filename) 
{
	ScopedLock lock(mutex);
	
	Log("-PCAPFile::open() [\"%s\"]\n",filename);
	
	//Open file
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600))<0)
		//Error
		return Error("-PCAPFile::open() | Could not open file [err:%d]\n",errno);
		
        //PCAP file header
	BYTE out[PCAP_HEADER_SIZE];
	
        set4(out, 0, PCAP_MAGIC_COOKIE);// Magic number used to detect byte order (In network order
        set2(out, 4, 0x02);		// Mayor
        set2(out, 6, 0x04);		// Minor
        set4(out, 8, 0);		// GMT to local correction
        set4(out, 12, 0);		// accuracy of timestamps
        set4(out, 16, 65535);		// max length of captured packets, in octets
        set4(out, 20, 1);		//data link type(ethernet)
	
	//Write it
	return write(fd, out, sizeof(out));
}
    
void PCAPFile::WriteUDP(QWORD currentTimeMillis,DWORD originIp, short originPort, DWORD destIp, short destPort,const BYTE* data, DWORD size, DWORD truncate)
{
	BYTE out[PCAP_UDP_PACKET_SIZE];
	
	DWORD saved = truncate ? std::min(truncate,size) : size;
	
	// Packet headers (16)
        set4(out,  0,( int) (currentTimeMillis/1000));             // timestamp seconds
        set4(out,  4, (int) ((currentTimeMillis %1000))*1000);     // timestamp in nanoseconds
        set4(out,  8, saved+42);                                   // number of octets of packet saved in file
        set4(out, 12, size+42);                                    // actual length of packet 
        //Write ehternet header (14)
	set6(out, 16, 0x00000000);
	set6(out, 22, 0x00000000);
        set2(out, 28, 0x0800);			// IPv4    
        //Write IP header (20)
        set1(out, 30, 0x45);                    // Version 4 Header Len 5
        set1(out, 31, 0x00);                    //Services
        set2(out, 32, size+28);                 // Length
        set2(out, 34, 0x00);                    // id
        set2(out, 36, 0x4000);                  // Flags Don't fragment
        set1(out, 38, 0x80);                    // TTL
        set1(out, 39, 0x11);                    // PROTO: UDP
        set2(out, 40, 0x00);                    // Header checksum
        set4(out, 42, originIp);                // Source
        set4(out, 46, destIp);                  // Destination
        //Write UDP (8)
        set2(out, 50, originPort);			
        set2(out, 52, destPort);
        set2(out, 54, size+8);
        set2(out, 56, 0x00);
	
	//Lock
	mutex.Lock();

        //Write header and content
	if (write(fd, out, sizeof(out))<0 || write(fd, data, saved)<0)
		//Error
		Error("-PCAPFile::WriteUDP() | Error writing file [errno:%d]\n",errno);
	
	//unlock
	mutex.Unlock();
}

void PCAPFile::Close()
{
	ScopedLock lock(mutex);
	
	//Check not already closed
	if (fd<0) return;

	Log("-PCAPFile::Close()\n");
	
	//Close file
	close(fd);
	fd = -1;
}
