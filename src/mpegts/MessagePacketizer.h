#ifndef MESSAGEPACKETIZER_H
#define MESSAGEPACKETIZER_H

#include "Buffer.h"
#include "BufferWritter.h"
#include "CircularQueue.h"
#include "log.h"

#include <stdint.h>
#include <list>
#include <mutex>
#include <atomic>
#include <cassert>

/**
 * The MessagePacketizer is a generic class to create packets for messages. It accepts T
 * messages as input and generate packets as requested. The early added message will be packetized
 * early.
 * 
 * Before packetization, the encode function of message will be called. The message wouldn't be encoded
 * until it is about to be packetized.
 * 
 * The generated packet size is dependent on the available space of the provided BufferWriter and the 
 * remaining bytes of the messages.
 * 
 * One generated packet could contain bytes from mutliple messages. The behaviour can be set by the
 * forceSeparatePacket flag. When it is true, the begining of current message will be packetized in
 * next packet. Otherwise, the bytes would be appended to current packet if current packet is partly
 * filled.
 * 
 * The MessagePacketizer uses a cicular queue to store added messages. The limit of the queue size is set during
 * construction. If the queue is full, the earlest message would be dropped.
 * 
 */
template<typename T>
class MessagePacketizer
{
public:
	
	/**
	 * Constructor
	 * 
	 * @param maxMessageQueueSize The max number of messages the packetizer can keep before they are packetized.
	 */
	MessagePacketizer(size_t maxMessageQueueSize) : messages(maxMessageQueueSize, false), buffer(0)
	{
	}
	
	/**
	 * Destructor
	 */
	virtual ~MessagePacketizer() = default;
	
	/**
	 * Add message for packetization.
	 * 
	 * @param message The message to be packetized. Note is is a shared pointer.
	 * @param forceSeparatePacket Whether the begining of the message must be at begining of a new packet.
	 */
	void AddMessage(const std::shared_ptr<T>& message, bool forceSeparatePacket = true)
	{
		if (messages.full())
		{
			Warning("-MessagePacketizer::AddMessage Message queue full. Dropping oldest message.");
		}
		
		messages.emplace_back(message, forceSeparatePacket);
		
		hasData = true;
	}
	
	/**
	 * Whether data is avaialble for packetization.
	 */
	inline bool HasData() const
	{
		return hasData;
	}

	/**
	 * Whether the next packet would be a start of a message.
	 */
	inline bool IsMessageStart() const
	{
		return pos == 0;
	}
	
	/**
	 * Get current message.
	 */
	std::shared_ptr<T> GetCurrentMessage() const
	{
		if (!current && !messages.empty())
			return messages.front().first;
			
		return current;
	}
	
	/**
	 * Write the next packet into the BufferWriter. The MessagePacketizer will try to 
	 * fill all the availabe space in the writer unless the left bytes of the message is
	 * less.
	 * 
	 * @return size_t The size of written bytes.
	 */
	virtual size_t WriteNextPacket(BufferWritter& writer)
	{
		size_t bytes = 0;
		
		while (writer.GetLeft())
		{
			if (buffer.IsEmpty() || pos >= buffer.GetSize())
			{
				if (messages.empty())
				{
					pos = 0;
					buffer.Reset();
					current.reset();
					hasData = false;
					
					return bytes;
				}
				
				// Grap the front message
				auto [msg, forceSeparate] = messages.front();
				current = msg;
				
				// Remove the message
				(void)messages.pop_front();
				
				// Create a new buffer for the message
				auto sz = msg->GetSize();
				buffer.SetSize(sz);
				
				BufferWritter awriter(buffer.GetData(), sz);
				msg->Serialize(awriter);
				assert(sz == awriter.GetLength());
				
				pos = 0;
				
				// check force sperate flag
				if (forceSeparate && bytes > 0)
				{
					return bytes;
				}
			}
			
			// Fill the writer as much as possible
			auto len = std::min(buffer.GetSize() - pos, writer.GetLeft());
			auto current = writer.Consume(len);
			memcpy(current, &buffer.GetData()[pos], len);
			
			pos += len;
			bytes += len;
		}
		
		return bytes;
	}
	
private:
	CircularQueue<std::pair<std::shared_ptr<T>, bool>> messages;
	
	Buffer buffer;
	size_t pos = 0;
	
	bool hasData = false;
	
	std::shared_ptr<T> current;
};

#endif
