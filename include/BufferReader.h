#ifndef LIBDATACHANNELS_INTERNAL_READER_H_
#define LIBDATACHANNELS_INTERNAL_READER_H_
#include <stdint.h>
#include <stddef.h>
#include <stdexcept>
#include <string>
#include <array>

#include "Buffer.h"

class BufferReader
{
public:
	BufferReader(Buffer& buffer)  :
		data(buffer.GetData()),
		size(buffer.GetSize())
	{
		this->pos = 0;
	}
	
	BufferReader(const uint8_t* data, const size_t size)  :
		data(data),
		size(size)
	{
		this->pos = 0;
	}
	
	template<std::size_t N> std::array<uint8_t,N> Get(size_t i) const 
	{
		std::array<uint8_t,N> array;
		memcpy(array.data(),data+i,N);
		return array;
	} 
	
	inline BufferReader GetReader(size_t i, size_t num) const 
	{ 
		return BufferReader(data+i,num);
	}
	
	inline std::string GetString(size_t i, size_t num) const 
	{ 
		return std::string((char*)data+i,num);
	}
	
	inline Buffer GetBuffer(size_t i, size_t num) const 
	{
		return Buffer(data+i,num);
	}
	
	inline uint8_t Get1(size_t i) const 
	{
		return data[i];
	}
	
	inline uint16_t Get2(size_t i) const 
	{ 
		return (uint16_t)(data[i+1]) | ((uint16_t)(data[i]))<<8; 
	}
	
	inline uint32_t Get3(size_t i) const 
	{
		return (uint32_t)(data[i+2]) | ((uint32_t)(data[i+1]))<<8 | ((uint32_t)(data[i]))<<16; 
	}
	
	inline uint32_t Get4(size_t i) const 
	{
		return (uint32_t)(data[i+3]) | ((uint32_t)(data[i+2]))<<8 | ((uint32_t)(data[i+1]))<<16 | ((uint32_t)(data[i]))<<24; 
	}
	
	inline uint64_t Get8(size_t i) const 
	{
		return ((uint64_t)Get4(i))<<32 | Get4(i+4);
	}

	template<std::size_t N> 
	std::array<uint8_t,N> Get() 			{ pos+=N; return Get<N>(pos-N);			}
	inline BufferReader GetReader(size_t num) 	{ pos+=num; return GetReader(pos-num, num);	}
	inline std::string  GetString(size_t num) 	{ pos+=num; return GetString(pos-num, num);	}
	inline Buffer	    GetBuffer(size_t num)	{ pos+=num; return GetBuffer(pos-num,num);	}
	inline uint8_t  Get1() 				{ auto val = Get1(pos); pos+=1; return val;	}
	inline uint16_t Get2() 				{ auto val = Get2(pos); pos+=2; return val;	}
	inline uint32_t Get3() 				{ auto val = Get3(pos); pos+=3; return val;	}
	inline uint32_t Get4() 				{ auto val = Get4(pos); pos+=4; return val;	}
	inline uint64_t Get8() 				{ auto val = Get8(pos); pos+=8; return val;	}
	inline uint8_t  Peek1()				{ return Get1(pos); }
	inline uint16_t Peek2()				{ return Get2(pos); }
	inline uint32_t Peek3()				{ return Get3(pos); }
	inline uint32_t Peek4()				{ return Get4(pos); }
	inline uint64_t Peek8()				{ return Get8(pos); }
	
	size_t   PadTo(size_t num)
	{
		size_t reminder = pos % num;
		if (reminder)
			pos += (num - reminder);
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
	const uint8_t* data;
	const size_t size;
	size_t pos;
};

#endif
