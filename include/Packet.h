#ifndef PACKET_H
#define PACKET_H

#include "config.h"
#include <cstring>
#include "Buffer.h"

class Packet : public Buffer
{
public:	
	Packet() : Buffer(MTU) {};
};

#endif /* PACKET_H */

