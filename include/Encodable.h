#ifndef ENCODABLE_H
#define ENCODABLE_H

#include <cstddef>

class BufferWritter;

class Encodable
{
public:
	virtual ~Encodable() = default;
	
	/**
	 * Encode the content
	 */
	virtual void Encode(BufferWritter& writer) = 0;
	
	/**
	 * The memory size needed for encoding. It must match
	 * the exact buffer used after encoding the content.
	 */
	virtual size_t Size() const = 0;
};

#endif
