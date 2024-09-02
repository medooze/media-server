#ifndef PACKETIZER_H
#define PACKETIZER_H

#include "Buffer.h"
#include "BufferWritter.h"

#include <stdint.h>
#include <list>

template<typename T>
class Packetizer
{
public:
	Packetizer()
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
	
	bool IsMessageStart() const
	{
		return !buffer || pos == buffer->GetSize();
	}
	
	virtual void GetNextPacket(BufferWritter& writer)
	{
		while (writer.GetLeft())
		{
			if (!buffer || pos >= buffer->GetSize())
			{
				if (messages.empty())
					return;
				
				// Grap the first message
				auto& msg = messages.begin();
				auto [encodable, forceSeparate] = msg;
				
				// check force sperate flag
				if (forceSeparate)
					return;
				
				// Create a new buffer for the message
				buffer = std::make_unique<Buffer>(encodable->Size());
				BufferWritter awriter(*buffer);
				encodable->Encode(awriter);
				
				buffer->SetSize(awriter.GetLength());
				pos = 0;
				
				// Remove the message
				messages.erase(messages.begin());
			}
			
			// Fill the writer as much as possible
			auto len = std::min(buffer->GetSize() - pos, writer.GetLeft());
			writer.SetN(pos, *buffer, len);
			
			pos += len;
		}
	}
	
private:
	std::list<std::pair<T, bool>> messages;
	
	std::unique_ptr<Buffer> buffer;
	size_t pos = 0;
};

#endif