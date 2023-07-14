#ifndef OBJECTPARSER_H
#define OBJECTPARSER_H

#include <vector>
#include <stdint.h>

class ObjectParser
{
public:
	ObjectParser(size_t objectSize) :
		objectSize(objectSize)
	{		
	}
	
	virtual ~ObjectParser() = default;
	
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
	
	void Reset()
	{
		buffer.clear();
	}
	
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