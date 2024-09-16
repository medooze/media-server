#ifndef PACKETIZER_H
#define PACKETIZER_H

#include "Buffer.h"
#include "BufferWritter.h"
#include "CircularQueue.h"
#include "Encodable.h"
#include "log.h"

#include <stdint.h>
#include <list>
#include <mutex>
#include <atomic>

template<typename T>
class Packetizer
{
public:
	
	Packetizer(size_t maxMessageQueueSize) : messages(maxMessageQueueSize, false)
	{
		static_assert(std::is_base_of_v<Encodable, T>);
	}
	
	virtual ~Packetizer() = default;
	
	void AddMessage(const std::shared_ptr<T>& message, bool forceSeparatePacket = false)
	{
		if (messages.full())
		{
			Warning("-Packetizer::AddMessage Message queue full. Dropping oldest message.");
		}
		
		messages.emplace_back(message, forceSeparatePacket);
		
		hasData = true;
	}
	
	inline bool HasData() const
	{
		return hasData;
	}

	inline bool IsMessageStart() const
	{
		return !buffer || pos == 0;
	}
	
	virtual size_t GetNextPacket(BufferWritter& writer)
	{
		size_t bytes = 0;
		
		while (writer.GetLeft())
		{
			if (!buffer || pos >= buffer->GetSize())
			{
				if (messages.empty())
				{
					pos = 0;
					
					hasData = false;
					return bytes;
				}
				
				// Grap the front message
				auto [encodable, forceSeparate] = messages.front();
				
				// Remove the message
				(void)messages.pop_front();
				
				// Create a new buffer for the message
				auto sz = encodable->Size();
				
				if (!buffer || sz > buffer->GetCapacity())
					buffer = std::make_unique<Buffer>(sz);
				
				BufferWritter awriter(buffer->GetData(), sz);
				encodable->Encode(awriter);
				
				buffer->SetSize(awriter.GetLength());
				
				// check force sperate flag
				if (forceSeparate && pos > 0)
				{
					pos = 0;
					return bytes;
				}
				
				pos = 0;
			}
			
			// Fill the writer as much as possible
			auto len = std::min(buffer->GetSize() - pos, writer.GetLeft());
			auto current = writer.Consume(len);
			memcpy(current, &buffer->GetData()[pos], len);
			
			pos += len;
			bytes += len;
		}
		
		return bytes;
	}
	
private:
	CircularQueue<std::pair<std::shared_ptr<T>, bool>> messages;
	
	std::unique_ptr<Buffer> buffer;
	size_t pos = 0;
	
	bool hasData = false;
};

#endif
