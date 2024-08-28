#ifndef BITREADER_H_
#define	BITREADER_H_

#include <stdexcept>
#include <cassert>

#include "config.h"
#include "tools.h"
#include "BufferReader.h"


class BitReader
{
public:
	BitReader()
	{
		//Nothing
		buffer = nullptr;
		bufferLen = 0;
		//nothing in the cache
		cached = 0;
		cache = 0;
		bufferPos = 0;
	}

	BitReader(const BYTE *data,const DWORD size)
	{
		//Store
		buffer = data;
		bufferLen = size;
		//nothing in the cache
		cached = 0;
		cache = 0;
		bufferPos = 0;
	}

	BitReader(BufferReader& reader, const DWORD size) :
		BitReader(reader.GetData(size), size)
	{
	}

	BitReader(const Buffer& reader) :
		BitReader(reader.GetData(), reader.GetSize())
	{
	}

	inline void Wrap(const BYTE *data,const DWORD size)
	{
		//Store
		buffer = data;
		bufferLen = size;
		//nothing in the cache
		cached = 0;
		cache = 0;
		bufferPos = 0;
	}
	
	inline void Release()
	{
		//Nothing
		buffer = nullptr;
		bufferLen = 0;
		//nothing in the cache
		cached = 0;
		cache = 0;
		bufferPos = 0;
	}
	
	inline void Reset()
	{
		//nothing in the cache
		cached = 0;
		cache = 0;
		bufferPos = 0;
	}

	inline DWORD Get(DWORD n)
	{
		assert(n <= 32);

		DWORD ret = 0;
			
		if (n>cached)
		{
			//What we have to read next
			BYTE a = n-cached;
			//Get remaining in the cache
			ret = cache >> (32-n);
			//Cache next
			Cache();
			//Get the remaining
			ret =  ret | GetCached(a);
		} else if (n) {
			//Get from cache
			ret = GetCached(n);
		}
		//Debug("Readed %d: cached:%d\n",n, cached);
		//BitDump(ret,n);
		return ret;
	}

	inline bool Check(int n, DWORD val)
	{
		return Get(n)==val;
	}

	inline void Skip(DWORD n)
	{
		assert(n <= 32);

		if (n>cached) 
		{
			//Get what is left to skip
			BYTE a = n-cached;
			//Cache next
			Cache();
			//Skip cache
			SkipCached(a);
		} else if (n) {
			//Skip cache
			SkipCached(n);
		}
		//Debug("Skiped n:%d: cached:%d\n", n, cached);
	}

	inline QWORD Left()
	{
		return QWORD(bufferLen - bufferPos) * 8 - cached;
	}

	inline DWORD Peek(DWORD n)
	{
		assert(n <= 32);

		DWORD ret = 0;

		if (n>cached) 
		{
			//What we have to read next
			BYTE a = n-cached;
			//Get remaining in the cache
			ret = cache >> (32-n);
			//Get the remaining
			ret =  ret | (PeekNextCached() >> (32-a));
		} else if (n) {
			//Get from cache
			ret = cache >> (32-n);
		}
		//Debug("Peeked %d:\n",n);
		//BitDump(ret,n);
		return ret;
	}

	inline DWORD GetPos()
	{
		return bufferPos*8-cached;
	}

	inline uint32_t GetNonSymmetric(uint8_t n) 
	{
		uint8_t w = 0;
		uint8_t x = n;
		while (x != 0) 
		{
			x = x >> 1;
			w++;
		}
		uint8_t m = (1 << w) - n;
		uint8_t v = Get(w - 1);
		if (v < m)
			return v;
		uint8_t extra_bit = Get(1);
		return (v << 1) - m + extra_bit;
	}

	inline uint32_t GetVariableLengthUnsigned()
	{
		int leadingZeros = 0;
		while (1)
		{
			bool done = Get(1);
			if (done)
				break;
			leadingZeros++;
		}
		if (leadingZeros >= 32)
			return UINT32_MAX;
		uint32_t value = Get(leadingZeros);
		return value + (1lu << leadingZeros) - 1;
	}

	inline DWORD GetExpGolomb()
	{
		//No len yet
		DWORD len = 0;
		//Count zeros
		while (!Get(1))
			//Increase len
			++len;
		//Check 0
		if (!len) return 0;
		//Get the exp
		DWORD value = Get(len);

		//Calc value
		return (1 << len) - 1 + value;
	}

	inline int GetExpGolombSigned()
	{
		//Get code num
		DWORD codeNum = GetExpGolomb();
		//Conver to signed
		return codeNum & 0x01 ? codeNum >> 1 : -(codeNum >> 1);
	}

	inline DWORD Flush()
	{
		Align();
		FlushCache();
		return bufferPos;
	}

	inline void FlushCache()
	{
		assert (cached <= bufferPos * 8);

		//Check if we have already finished
		if (!cached)
			//exit
			return;

		//BitDump(cache,cached);
		// We need to return the cached bits to the buffer
		auto bytes = cached / 8;
		//Debug("Flushing Cache cached:%d bytes:%d len:%u pos:%u\n", cached, bytes, bufferLen, bufferPos);

		//Increase pointers
		bufferLen += bytes;
		buffer -= bytes;
		bufferPos -= bytes;

		//Nothing cached
		cached = 0;
		cache = 0;
		//Debug("Flushed cache len:%u pos:%u\n", bufferLen, bufferPos);
	}

	inline void Align()
	{
		if (cached % 8 == 0)
			return;

		if (cached > 24)
			Skip(32 - cached);
		else if (cached > 16)
			Skip(24 - cached);
		else if (cached > 8)
			Skip(16 - cached);
		else
			Skip(8 - cached);
	}

private:
	inline DWORD Cache()
	{
		//Check left buffer
		if (bufferLen-bufferPos>3)
		{
			//Update cache
			cache = get4(buffer+bufferPos,0);
			//Update bit count
			cached = 32;
			//Increase pointer
			bufferPos += 4;

		} else if(bufferLen-bufferPos==3) {
			//Update cache
			cache = get3(buffer+bufferPos,0)<<8;
			//Update bit count
			cached = 24;
			//Increase pointer
			bufferPos += 3;
		} else if (bufferLen-bufferPos==2) {
			//Update cache
			cache = get2(buffer+bufferPos,0)<<16;
			//Update bit count
			cached = 16;
			//Increase pointer
			bufferPos += 2;
		} else if (bufferLen-bufferPos==1) {
			//Update cache
			cache  = get1(buffer+bufferPos,0)<<24;
			//Update bit count
			cached = 8;
			//Increase pointer
			bufferPos++;
		} else {
			throw std::out_of_range("no more bytes to read from");
		}
			

		//Debug("Reading int cache %x:%d\n",cache,cached);
		//BitDump(cache>>(32-cached),cached);

		//return number of bits
		return cached;
	}

	inline DWORD PeekNextCached()
	{
		//Check left buffer
		if (bufferLen-bufferPos>3)
		{
			//return  cached
			return get4(buffer+bufferPos,0);
		} else if(bufferLen-bufferPos==3) {
			//return  cached
			return get3(buffer+bufferPos,0)<<8;
		} else if (bufferLen-bufferPos==2) {
			//return  cached
			return get2(buffer+bufferPos,0)<<16;
		} else if (bufferLen-bufferPos==1) {
			//return  cached
			return get1(buffer+bufferPos,0)<<24;
		} else {
			throw std::out_of_range("no more bytes to read from");
		}
	}

	inline void SkipCached(DWORD n)
	{
		assert(n <= cached);

		//Check length
		if (!n) return;

		if (n < 32)
		{
			//Move
			cache = cache << n;
			//Update cached bytes
			cached -= n;
		} else {
			//cached == 32
			cache = 0;
			cached = 0;
		}
			
	}

	inline DWORD GetCached(DWORD n)
	{
		assert(n>cached);

		//Check length
		if (!n) return 0;
		//Get bits
		DWORD ret = cache >> (32-n);
		//Skip those bits
		SkipCached(n);
		//Return bits
		return ret;
	}
	
private:
	const BYTE* buffer;
	DWORD bufferLen;
	DWORD bufferPos;
	DWORD cache;
	BYTE  cached;
};


#endif	/* BITREADER_H_ */
