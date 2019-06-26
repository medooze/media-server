#ifndef PACKET_H
#define PACKET_H

#include "config.h"
#include <cstring>

class Packet
{
public:	
	const uint8_t* GetData() const		{ return buffer.data();		}
	uint8_t* GetData()			{ return buffer.data();		}
	size_t GetCapacity() const		{ return capacity;		}
	size_t GetSize() const			{ return size;			}

	void SetSize(size_t size) 
	{ 
		//Check capacity
		if (size>capacity)
			return;
		//Set new size
		this->size = size;
	}

	void SetData(const uint8_t* data,const size_t size)
	{
		//Check size
		if (size>capacity)
			return;
		//Copy
		std::memcpy(buffer.data(),data,size);
		//Reset size
		this->size = size;
	}
	
	void SetData(const Packet& packet)
	{
		SetData(packet.GetData(),packet.GetSize());
	}
private:
	static const DWORD SIZE = 1700;
protected:
	std::array<uint8_t,SIZE> buffer;
	size_t capacity		= SIZE;
	size_t size		= 0;
};

#endif /* PACKET_H */

