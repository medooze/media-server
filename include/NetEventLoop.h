#ifndef NETEVENTLOOP_H
#define NETEVENTLOOP_H

#include "config.h"
#include "Packet.h"
#include "ObjectPool.h"
#include "EventLoop.h"
#include "PacketHeader.h"

using namespace std::chrono_literals;

class NetEventLoop : public EventLoop
{
public:
	class Listener
	{
	public:
		virtual ~Listener() = default;
		virtual void OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port) = 0;
	};

	NetEventLoop(Listener* listener = nullptr, uint32_t packetPoolSize = 0);
	virtual ~NetEventLoop() = default;
	
	virtual bool SetAffinity(int cpu) override;
	
	void Send(const uint32_t ipAddr, const uint16_t port, Packet&& packet, const std::optional<PacketHeader::FlowRoutingInfo>& rawTxData = std::nullopt, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback = std::nullopt);
	
	void SetRawTx(const FileDescriptor &fd, const PacketHeader& header, const PacketHeader::FlowRoutingInfo& defaultRoute);
	void ClearRawTx();
	
	ObjectPool<Packet>& GetPacketPool() { return packetPool; }
	
protected:
	
	virtual std::optional<uint16_t> GetPollEventMask(int pfd) const override;
	virtual void OnPollIn(int pfd) override;
	virtual void OnPollOut(int pfd) override;
	virtual void OnPollError(int pfd, const std::string& errorMsg) override;

private:
	enum State
	{
		Normal,
		Lagging,
		Overflown
	};
	
	struct RawTx
	{
		FileDescriptor fd;
		PacketHeader header;
		PacketHeader::FlowRoutingInfo defaultRoute;

		RawTx(const FileDescriptor& fd, const PacketHeader& header, const PacketHeader::FlowRoutingInfo& defaultRoute)	:
			fd(fd),
			header(header),
			defaultRoute(defaultRoute)
		{
		}
	};

	struct SendBuffer
	{
		//Don't allocate packet on default constructor
		SendBuffer() :
			packet(0)
		{
		}
		
		SendBuffer(uint32_t ipAddr, uint16_t port, const std::optional<PacketHeader::FlowRoutingInfo>& rawTxData, Packet&& packet, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback) :
			ipAddr(ipAddr),
			port(port),
			packet(std::move(packet)),
			rawTxData(rawTxData),
			callback(callback)
		{
		}
		SendBuffer(SendBuffer&& other) :
			ipAddr(other.ipAddr),
			port(other.port),
			packet(std::move(other.packet)),
			rawTxData(other.rawTxData),
			callback(other.callback)
		{
		}
		SendBuffer& operator=(SendBuffer&&) = default;
		//NO copyable
		SendBuffer(const SendBuffer&) = delete;
		SendBuffer& operator=(SendBuffer const&) = delete;
		
		uint32_t ipAddr = 0;
		uint16_t port = 0;
		Packet   packet;
		std::optional<PacketHeader::FlowRoutingInfo> rawTxData;
		std::optional<std::function<void(std::chrono::milliseconds)>> callback;
		
	};

	static constexpr size_t MaxSendingQueueSize = 64*1024;
	static constexpr size_t PacketPoolSize = 1024;

	static constexpr size_t MaxMultipleSendingMessages = 128;
	static constexpr size_t MaxMultipleReceivingMessages = 128;

	Listener*	listener	= nullptr;
	State		state		= State::Normal;
	moodycamel::ConcurrentQueue<SendBuffer>	sending;
	ObjectPool<Packet> packetPool;
	std::optional<RawTx> rawTx;
	
	//Recv data
	uint8_t datas[MaxMultipleReceivingMessages][MTU] ZEROALIGNEDTO32;
	size_t  size = MTU;
	
	//UDP send flags
	const uint32_t flags = MSG_DONTWAIT;
	
	//Pending data
	std::vector<SendBuffer> items;
};

#endif /* EVENTLOOP_H */

