#ifndef UDPREADER_H
#define UDPREADER_H

class UDPReader
{
public:
	virtual ~UDPReader() {};
	//Abscract interface
	virtual uint64_t Next() = 0;
	virtual uint8_t* GetUDPData() const = 0;
	virtual uint32_t GetUDPSize() const = 0;
	virtual uint64_t Seek(const uint64_t time) = 0;
	virtual void Rewind() = 0;
	virtual bool Close() = 0;
};

#endif /* UDPREADER_H */

