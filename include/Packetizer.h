#ifndef PACKETIZER_H
#define PACKETIZER_H

#include "Buffer.h"
#include "BufferWritter.h"
#include "CircularQueue.h"
#include "log.h"

#include <stdint.h>
#include <list>
#include <mutex>
#include <atomic>

template<typename T>
class Packetizer
{
public:
	static constexpr size_t MaxMesssageQueueSize = 256;
	
	Packetizer() : messages(MaxMesssageQueueSize, false)
	{
	}
	
	void AddMessage(const T& message, bool forceSeparatePacket = false)
	{
		std::lock_guard lock(mutex);
		
		if (messages.full())
		{
			Warning("-Packetizer::AddMessage Message queue full. Dropping oldes message.");
		}
		
		messages.emplace_back(message, forceSeparatePacket);
		
		hasData.store(true, std::memory_order_release);
	}
	
	inline bool HasData() const
	{
		return hasData.load(std::memory_order_acquire);
	}

	// Note IsMessageStart() and GetNextPacket must be called in same thread

	inline bool IsMessageStart() const
	{
		return !buffer || pos == 0;
	}
	
	void GetNextPacket(BufferWritter& writer)
	{
		while (writer.GetLeft())
		{
			if (!buffer || pos >= buffer->GetSize())
			{
				std::unique_lock lock(mutex);
				
				if (messages.empty())
				{
					pos = 0;
					
					hasData.store(false, std::memory_order_release);
					return;
				}
				
				// Grap the first message
				auto& msg = messages.front();
				auto [encodable, forceSeparate] = msg;
				
				// Remove the message
				(void)messages.pop_front();
				
				lock.unlock();
				
				// Create a new buffer for the message
				auto sz = encodable->Size();
				
				if (sz > buffer->GetCapacity())
					buffer = std::make_unique<Buffer>(sz);
				
				BufferWritter awriter(buffer->GetData(), sz);
				encodable->Encode(awriter);
				
				buffer->SetSize(awriter.GetLength());
				
				// check force sperate flag
				if (forceSeparate && pos > 0)
				{
					pos = 0;
					return;
				}
				
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
	CircularQueue<std::pair<T, bool>> messages;
	
	mutable std::mutex mutex;
	
	std::unique_ptr<Buffer> buffer;
	size_t pos = 0;
	
	std::atomic<bool> hasData = { false };
};

#endif
