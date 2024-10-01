#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include <cstddef>

class BufferWritter;

class Serializable
{
public:
	virtual ~Serializable() = default;
	
	/**
	 * Serialize the content
	 */
	virtual void Serialize(BufferWritter& writer) const = 0;
	
	/**
	 * The memory size needed for encoding. It must match
	 * the exact buffer used after encoding the content.
	 */
	virtual size_t GetSize() const = 0;
};

#endif
