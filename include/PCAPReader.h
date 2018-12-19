
#ifndef PCAPMEDIAFILE_H
#define PCAPMEDIAFILE_H

#include "config.h"
#include "log.h"
#include "UDPReader.h"

class PCAPReader : 
	public UDPReader
{
public:
	PCAPReader();
	virtual ~PCAPReader();
	bool Open(const char* file);
	
	//UDPReader interface
	virtual uint64_t Next() override;
	virtual uint8_t* GetUDPData() const override { return packet;	 }
	virtual uint32_t GetUDPSize() const override { return packetLen; }
	virtual uint64_t Seek(const uint64_t time) override;
	virtual void Rewind() override;
	virtual bool Close() override;;

private:
	uint8_t data[65535];
	size_t size = 65535;
	uint8_t* packet = nullptr;
	uint32_t packetLen = 0;
	int fd = -1;
};

#endif /* PCAPMEDIAFILE_H */

