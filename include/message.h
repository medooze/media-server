#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <cstring>
#include <stddef.h>
#include <memory>

namespace data {

enum MessageType
{
	Unkown = 0,
	Scte35 = 1,
};

class MessageInfo final
{
public:
	MessageType type = MessageType::Unkown;
	
	// Port and streamId together identify the data channel
	uint32_t port = 0;
	uint32_t streamId = 0;
};

class Message final
{
public:
	Message(MessageType type = MessageType::Unkown, uint32_t port = 0, uint32_t streamId = 0)
	{	
		info.type = type;
		info.port = port;
		info.streamId = streamId;
	}

	Message(const Message& other)
	{
		info = other.info;
		data = static_cast<uint8_t*>(malloc(other.len));
		std::memcpy(data, other.data, other.len);
		len = other.len;
	}
	
	Message(Message&& other)
	{
		info = other.info;
		data = other.data;
		len = other.len;
		
		other.info.type = MessageType::Unkown;
		other.info.port = 0;
		other.info.streamId = 0;
		other.data = nullptr;
		other.len = 0;
	}

	Message& operator=(const Message& other)
	{
		if (this != &other)
		{
			info = other.info;
			data = static_cast<uint8_t*>(malloc(other.len));
			std::memcpy(data, other.data, other.len);
			len = other.len;
		}
		
		return *this;
	}

	Message& operator=(Message&& other)
	{
		if (this != &other)
		{
			info = other.info;
			data = other.data;
			len = other.len;
			
			other.info.type = MessageType::Unkown;
			other.info.port = 0;
			other.info.streamId = 0;
			other.data = nullptr;
			other.len = 0;
		}
		
		return *this;
	}
	
	~Message()
	{
		delete data;
		len = 0;
	}
	
	void SetData(const uint8_t* data, size_t len)
	{
		if (this->data != nullptr)
		{
			delete this->data;
		}
		
		this->len = len;
		
		this->data = static_cast<uint8_t*>(malloc(len));
		std::memcpy(this->data, data, len);
	}

	MessageInfo GetInfo() const
	{
		return info;
	}
	
	const uint8_t* GetData() const
	{
		return data;
	}
	
	size_t GetLength() const
	{
		return len;
	}

private:
	MessageInfo info;
	
	uint8_t* data = nullptr;
	size_t len = 0;
};

class MessageListener
{
public:
	using shared = std::shared_ptr<MessageListener>;
	
	virtual ~MessageListener() = default;
	virtual void OnMessage(const Message& message) = 0;
};

class MessageProducer
{
public:
	virtual ~MessageProducer() = default;
	virtual void AddMessageListener(const MessageListener::shared& listener) = 0;
	virtual void RemoveMessageListener(const MessageListener::shared& listener) = 0;
};

}


#endif
