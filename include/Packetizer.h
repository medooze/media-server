#ifndef PACKETIZER_H
#define PACKETIZER_H

#include "Buffer.h"
#include "media.h"
#include "psi.h"

#include <vector>
#include <map>
#include <stdint.h>
#include <list>

template<typename T>
class Packetizer
{
public:
	Packetizer(size_t maxPacketSize) : maxPacketSize(maxPacketSize)
	{
	}
	
	void AddMessage(const T& message, bool forceSeparatePacket = false)
	{
		messages.emplace_back(message, forceSeparatePacket);
	}
	
	bool HasData() const
	{
		if (!messages.empty()) return true;
		
		return buffer && pos < buffer->GetSize();
	}
	
	virtual void GetNextPacket(BufferWritter& writter)
	{
		if (maxPacketSize == 0)
			throw std::runtime_error("max packet size is not set");
			
		if (writter.GetLeft() < maxPacketSize)
			throw std::runtime_error("Not enough buffer for generating packet.");
		
		while (true)
		{
			if (!buffer || pos >= buffer->GetSize())
			{
				if (messages.empty())
					return;
				
				// check force sperate flag
				if (messages.begin()->second)
					return;
					
				buffer = messages.begin()->first->Encode();
				pos = 0;
				
				messages.erase(message.begin());
			}
			
			auto len = std::min(buffer->GetSize() - pos, maxPacketSize);
			writter.SetN(pos, buffer, len);
			
			pos += len;
		}
	}
	
private:
	size_t maxPacketSize = 0;
	
	std::list<std::pair<T, bool>> messages;
	
	std::unique_ptr<Buffer> buffer;
	size_t pos = 0;
};

#endif