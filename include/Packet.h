#ifndef PACKET_H
#define PACKET_H

#include "config.h"
#include "log.h"
#include <cstring>
#include "Buffer.h"

class Packet
{
private:
	static const DWORD PREFIX = 200;
public:	
	Packet(std::size_t size = MTU) 
		: buffer(size ? size + PREFIX : 0)
	{
		Reset();
	};
	~Packet() 
	{
	};


	void Reset()
	{
		//reset buffer
		buffer.Reset();
		//If not empty buffer
		if (buffer.GetCapacity())
		{
			//Reset data
			data = buffer.GetData() + PREFIX;
			size = 0;
		}
	}

	Packet(const Packet&) = delete;
	Packet(Packet&& other)  noexcept
	{
		this->buffer = std::move(other.buffer);
		this->data = other.data;
		this->size = other.size;
		other.data = nullptr;
		other.size = 0;
	}

	Packet& operator=(Packet const&) = delete;
	Packet& operator=(Packet&& other) noexcept
	{
		this->buffer = std::move(other.buffer);
		this->data = other.data;
		this->size = other.size;
		other.data = nullptr;
		other.size = 0;
		return *this;
	}


	uint8_t* GetData()		const { return data; }
	size_t   GetSize()		const { return size; }
	size_t   GetCapacity()		const { return buffer.GetCapacity() - GetPrefixCapacity(); }
	size_t   GetPrefixCapacity()	const { return data - buffer.GetData(); }
	

	bool SetSize(size_t size)
	{
		//Check size
		if (size > GetCapacity())
			//Error
			return false;
		//Set new size
		this->size = size;
		//good
		return true;
	}

	// SetData overrides any preffixed data
	void SetData(const uint8_t* data, const size_t size)
	{
		//Reset buffer to restore prefix
		Reset();
		//Check size available for data
		if (size > GetCapacity())
		{
			//Allocate new size, with prefix
			buffer.Alloc(size + PREFIX);
			//Reset again
			Reset();
		}
		//Copy
		memcpy(this->data, data, size);
		//Set data size
		this->size = size;
	}

	// SetData overrides any preffixed data
	void SetData(const Buffer& buffer)
	{
		SetData(buffer.GetData(), buffer.GetSize());
	}

	void AppendData(const uint8_t* data, const size_t size)
	{
		//Check size available for data
		if (this->size + size > GetCapacity())
			//Allocate new size, keeping the prefix
			buffer.Alloc(this->size + size + GetPrefixCapacity());
		//Copy
		std::memcpy(this->data + this->size, data, size);
		//Increase data size
		this->size += size;
	}

	bool PrefixData(BYTE* data, DWORD size)
	{
		//Check size of prefixable data
		if (size > GetPrefixCapacity())
			//Error
			return false;
		//Copy
		memcpy(this->data - size, data, size);
		//Set pointers
		this->data -= size;
		this->size += size;
		//good
		return true;
	}


private:
	Buffer	buffer;
	BYTE*	data	= nullptr;
	DWORD	size	= 0;
};

#endif /* PACKET_H */

