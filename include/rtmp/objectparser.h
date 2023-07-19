#ifndef OBJECTPARSER_H
#define OBJECTPARSER_H

#include <vector>
#include <stdint.h>

/**
 * A class to help parsing a fixed size object from chunked data. The object
 * can be an opaque block of data.
 * 
 * When parsing a new block of data, it would try to use as much data as needed
 * to fill the desired buffer.
 * 
 */
class ObjectParser
{
public:
	/**
	 * Constructor
	 * 
	 * @param objectSize The object size.
	 */
	ObjectParser(size_t objectSize) :
		objectSize(objectSize)
	{		
	}
	
	/**
	 * Destructor
	 */
	virtual ~ObjectParser() = default;
	
	/**
	 * Parse a chunk of data
	 * 
	 * @param data The data to be parsed.
	 * @param len Length of the data
	 * 
	 * @return An pair. The first element is whether the object has been parsed. The second
	 *         element is the length of data that was used.
	 */
	std::pair<bool, size_t> Parse(uint8_t* data, size_t len)
	{
		if (buffer.size() >= objectSize)
		{
			return {true, 0};
		}
		
		size_t parsed = std::min(objectSize - buffer.size(), len);
		
		buffer.insert(buffer.end(), data, data + parsed);
		
		return { objectSize == buffer.size(), parsed };
	}
	
	/**
	 * Get the object buffer
	 */
	const std::vector<uint8_t>& GetBuffer() const
	{
		return buffer;
	}

protected:

	std::vector<uint8_t> buffer;
	
private:
	
	size_t objectSize;
};

#endif