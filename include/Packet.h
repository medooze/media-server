#ifndef PACKET_H
#define PACKET_H

#include "config.h"
#include "log.h"
#include <cstring>
#include "Buffer.h"

class Packet : public Buffer
{
public:	
	Packet(std::size_t size = MTU) : Buffer(size) {};
	~Packet() {};

	Packet(const Packet&)			= delete;
	Packet(Packet&&)			= default;

	Packet& operator=(Packet const&)	= delete;
	Packet& operator=(Packet&&)		= default;

	
};

#endif /* PACKET_H */

