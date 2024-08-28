#ifndef BITREADER_H_
#define	BITREADER_H_
#include "config.h"
#include "tools.h"
#include <stdexcept>
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
		//No error
		error = false;
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
		//No error
		error = false;
	}

	BitReader(BufferReader& reader, const DWORD size) :
		BitReader(reader.GetData(size), size)
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
		//No error
		error = false;
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
		//No error
		error = false;
	}
	
	inline void Reset()
	{
		//nothing in the cache
		cached = 0;
		cache = 0;
		bufferPos = 0;
		//No error
		error = false;
	}
	inline DWORD Get(DWORD n)
	{
		DWORD ret = 0;
		if (n>32) {
			//We can't use exceptions so set error flag
			error = true;
		} else if (n>cached){
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

	inline bool Check(int n,DWORD val)
	{
		return Get(n)==val;
	}

	inline void Skip(DWORD n)
	{
		if (n>32) {
			//We can't use exceptions so set error flag
			error = true;
		} else if (n>cached) {
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
		DWORD ret = 0;
		if (n>32) {
			//We can't use exceptions so set error flag
			error = true;
		} else if (n>cached) {
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
			if (error)
				return UINT32_MAX;
			if (done)
				break;
			leadingZeros++;
		}
		if (leadingZeros >= 32)
			return UINT32_MAX;
		uint32_t value = Get(leadingZeros);
		return value + (1lu << leadingZeros) - 1;
	}

	inline DWORD Flush()
	{
		Align();
		FlushCache();
		return bufferPos;
	}

	inline void FlushCache()
	{
		//Check if we have already finished
		if (!cached)
			//exit
			return;
		//Check size
		if (cached > bufferPos * 8)
		{
			//We can't use exceptions so set error flag
			error = true;
			//Exit
			return;
		}
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

	inline bool Error()
	{
		//We won't use exceptions, so we need to signal errors somehow
		return error;
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
			//We can't use exceptions so set error flag
			error = true;
			//Exit
			return 0;
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
			//We can't use exceptions so set error flag
			error = true;
			//Exit
			return 0;
		}
	}

	inline void SkipCached(DWORD n)
	{
		//Check length
		if (!n) return;
		if (n > cached)
		{
			error = true;
		} else if (n < 32) {
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
		//Check length
		if (!n) return 0;
		//Check available
		if (cached<n)
		{
			error = true;
			return UINT32_MAX;
		}
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
	bool  error;
};


#endif	/* BITREADER_H_ */
