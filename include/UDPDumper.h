#ifndef UDPDUMPER_H
#define UDPDUMPER_H

class UDPDumper
{
public:
	virtual ~UDPDumper() = default;
	virtual void WriteUDP(uint64_t currentTimeMillis,uint32_t originIp, short originPort, uint32_t destIp, short destPort,const uint8_t* data, uint32_t size, DWORD truncate = 0) = 0;
	virtual void Close() = 0;
};

#endif /* UDPDUMPER_H */

