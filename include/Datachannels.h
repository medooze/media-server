#ifndef LIBDATACHANNELS_H_
#define LIBDATACHANNELS_H_
#include <stdint.h>
#include <chrono>
#include <memory>
#include <string>
#include <functional>
#include "TimeService.h"

namespace datachannels
{

using Timer = ::Timer;
using TimeService = ::TimeService;

class Datachannel
{
public:
	enum MessageType
	{
		UTF8,
		Binary
	};
	
	struct Options
	{
		
	};

	using shared = std::shared_ptr<Datachannel>;
public:
	virtual ~Datachannel() = default;
	virtual bool Send(MessageType type, const uint8_t* data = nullptr, const uint64_t size = 0)  = 0;
	virtual bool Close() = 0;
	
	// Event handlers
	virtual void OnMessage(const std::function<void(MessageType, const uint8_t*,uint64_t)>& callback) = 0;
	
};

enum Setup
{
	Client,
	Server
};
	
class Transport
{
public:
	virtual ~Transport() = default;
	virtual size_t ReadPacket(uint8_t *data, uint32_t size) = 0;
	virtual size_t WritePacket(uint8_t *data, uint32_t size) = 0; 
	
	virtual void OnPendingData(std::function<void(void)> callback) = 0;
};

class Endpoint
{
public:
	struct Options
	{
		uint16_t localPort	= 5000;
		uint16_t remotePort	= 5000;
		Setup setup		= Server;
	};
	
	using shared = std::shared_ptr<Endpoint>;
public:
	// Static methods
	
	// Create new datachannel endpoint
	//	options.setup : Client/Server	
	static Endpoint::shared Create(TimeService& timeService) ;
	
public:
	virtual ~Endpoint() = default;
	virtual bool Init(const Options& options) = 0;
	virtual Datachannel::shared CreateDatachannel(const Datachannel::Options& options)  = 0;
	virtual bool Close()  = 0;
	
	// Getters
	virtual uint16_t   GetLocalPort() const = 0;
	virtual uint16_t   GetRemotePort() const = 0;
	virtual Transport& GetTransport() = 0;
};

}; //namespace

#endif
