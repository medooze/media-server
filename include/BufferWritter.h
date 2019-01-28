#ifndef LIBDATACHANNELS_INTERNAL_WRITTER_H_
#define LIBDATACHANNELS_INTERNAL_WRITTER_H_
#include <stdint.h>
#include <stddef.h>
#include <array>
#include <string>

#include "Buffer.h"

class BufferWritter
{
public:
	BufferWritter(Buffer& buffer)
	{
		this->data = buffer.GetData();
		this->size = buffer.GetSize();
		this->pos = 0;
	}
	
	BufferWritter(uint8_t* data, size_t size)
	{
		this->data = data;
		this->size = size;
		this->pos = 0;
	}
	inline BufferWritter GetWritter(size_t num) 	{ pos+=num; return GetWritter(pos-num, num);	}
	inline BufferWritter GetWritter(size_t i,size_t num) 	{ return BufferWritter(data+i, num);	}
	
	template<std::size_t N> 
	inline size_t Set(const std::array<uint8_t,N>& array)	 { Set(pos,array);  return pos+=N;			}
	inline size_t Set(const Buffer& buffer)			 { Set(pos,buffer); return pos+=buffer.GetSize();	}
	inline size_t Set(const std::string& string)		 { Set(pos,string); return pos+=string.length();	}
	inline size_t Set1(uint8_t val)	  { Set1(pos,val); return pos+=1; }
	inline size_t Set2(uint16_t val)  { Set2(pos,val); return pos+=2; }
	inline size_t Set3(uint32_t val)  { Set3(pos,val); return pos+=3; }
	inline size_t Set4(uint32_t val)  { Set4(pos,val); return pos+=4; }
	inline size_t Set6(uint64_t val)  { Set6(pos,val); return pos+=6; }
	inline size_t Set8(uint64_t val)  { Set8(pos,val); return pos+=8; }
	
	
	template<std::size_t N> 
	inline void Set(size_t i, const std::array<uint8_t,N>& array) 
	{
		memcpy(data+i,array.data(),N);
	}
	
	inline void Set(size_t i, const Buffer& buffer)
	{
		memcpy(data+i,buffer.GetData(),buffer.GetSize());
	}
	
	inline void Set(size_t i, const std::string& string)  
	{
		memcpy(data+i,string.data(),string.length());
	}
	
	inline void Set1(size_t i, uint8_t val)
	{
		data[i] = val;
	}
	
	inline void Set2(size_t i, uint16_t val)
	{
		data[i+1] = (uint8_t)(val);
		data[i]   = (uint8_t)(val>>8);
	}
	
	inline void Set3(size_t i, uint32_t val)
	{
		data[i+2] = (uint8_t)(val);
		data[i+1] = (uint8_t)(val>>8);
		data[i]   = (uint8_t)(val>>16);
	}
	
	inline void Set4(size_t i, uint32_t val)
	{
		data[i+3] = (uint8_t)(val);
		data[i+2] = (uint8_t)(val>>8);
		data[i+1] = (uint8_t)(val>>16);
		data[i]   = (uint8_t)(val>>24);
	}

	inline void Set6(size_t i, uint64_t val)
	{
		data[i+5] = (uint8_t)(val);
		data[i+4] = (uint8_t)(val>>8);
		data[i+3] = (uint8_t)(val>>16);
		data[i+2] = (uint8_t)(val>>24);
		data[i+1] = (uint8_t)(val>>32);
		data[i]   = (uint8_t)(val>>40);
	}

	inline void Set8(size_t i, uint64_t val)
	{
		data[i+7] = (uint8_t)(val);
		data[i+6] = (uint8_t)(val>>8);
		data[i+5] = (uint8_t)(val>>16);
		data[i+4] = (uint8_t)(val>>24);
		data[i+3] = (uint8_t)(val>>32);
		data[i+2] = (uint8_t)(val>>40);
		data[i+1] = (uint8_t)(val>>48);
		data[i]   = (uint8_t)(val>>56);
	}
	
	size_t PadTo(size_t num, uint8_t val = 0)
	{
		while (pos % num && pos<size)
			data[pos++] = val;
		return pos;
	}

	bool   Assert(size_t num) const 	{ return pos+num<size;	}
	void   GoTo(size_t mark) 		{ pos = mark;		}
	size_t Skip(size_t num) 		{ size_t mark = pos; pos += num; return mark;	}
	int64_t  GetOffset(size_t mark) const 	{ return pos-mark;	}
	size_t Mark() const 			{ return pos;		}
	size_t GetLength() const 		{ return pos;		}
	size_t GetLeft() const 			{ return size-pos;	}
	size_t GetSize() const 			{ return size;		}
	
private:
	uint8_t* data;
	size_t size;
	size_t pos;
};

#endif 
