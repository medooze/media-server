#ifndef PACKETIZER_H
#define PACKETIZER_H

#include "Buffer.h"
#include "BufferWritter.h"

#include <stdint.h>
#include <list>
#include <mutex>

template<typename T>
class Packetizer
{
public:
	Packetizer()
	{
	}
	
	void AddMessage(const T& message, bool forceSeparatePacket = false)
	{
		std::lock_guard lock(mutex);
		
		messages.emplace_back(message, forceSeparatePacket);
	}
	
	bool HasData() const
	{
		std::lock_guard lock(mutex);
		
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
				std::unique_lock lock(mutex);
				
				if (messages.empty())
					return;
				
				// Grap the first message
				auto& msg = messages.front();
				auto [encodable, forceSeparate] = msg;
				
				// check force sperate flag
				if (forceSeparate && buffer && pos != 0)
				{
					pos = 0;
					buffer.reset();
					return;
				}
				
				// Remove the message
				messages.erase(messages.begin());
				
				lock.unlock();
				
				// Create a new buffer for the message
				auto sz = encodable->Size();
				buffer = std::make_unique<Buffer>(sz);
				BufferWritter awriter(buffer->GetData(), sz);
				encodable->Encode(awriter);
				
				buffer->SetSize(awriter.GetLength());
				pos = 0;
			}
			
			// Fill the writer as much as possible
			auto len = std::min(buffer->GetSize() - pos, writer.GetLeft());
			auto current = writer.Consume(len);
			memcpy(current, &buffer->GetData()[pos], len);
			
			pos += len;
		}
	}
	
private:
	std::list<std::pair<T, bool>> messages;
	
	mutable std::mutex mutex;
	
	std::unique_ptr<Buffer> buffer;
	size_t pos = 0;
};

#endif
