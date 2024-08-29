#ifndef BITWRITTER_H_
#define	BITWRITTER_H_
#include "config.h"
#include "tools.h"
#include <stdexcept>
#include "BufferWritter.h"

class BitWritter {
public:
	BitWritter(BYTE *data,DWORD size)
	{
		//Store pointers
		this->data = data;
		this->size = size;
		//And reset to init values
		Reset();
	}

	BitWritter(BufferWritter& writter, DWORD size) : 
		BitWritter(writter.Consume(size), size)
	{
	}

	inline void Reset()
	{
		//Init
		buffer = data;
		bufferLen = 0;
		bufferSize = size;
		//nothing in the cache
		cached = 0;
		cache = 0;
		//No error
		error = false;
	}

	inline DWORD Flush()
	{
		Align();
		FlushCache();
		return bufferLen;
	}
	
	inline void FlushCache()
	{
		//Check if we have already finished
		if (!cached)
			//exit
			return;
		//Check size
		if (cached>bufferSize*8)
		{
			//We can't use exceptions so set error flag
			error = true;
			//Exit
			return;
		}
		//Debug("Flushing  cache");
		//BitDump(cache,cached);
		if (cached==32)
		{
			//Set data
			set4(buffer,0,cache);
			//Increase pointers
			bufferSize-=4;
			buffer+=4;
			bufferLen+=4;
		} else if (cached==24) {
			//Set data
			set3(buffer,0,cache);
			//Increase pointers
			bufferSize-=3;
			buffer+=3;
			bufferLen+=3;
		}else if (cached==16) {
			set2(buffer,0,cache);
			//Increase pointers
			bufferSize-=2;
			buffer+=2;
			bufferLen+=2;
		}else if (cached==8) {
			set1(buffer,0,cache);
			//Increase pointers
			--bufferSize;
			++buffer;
			++bufferLen;
		}
		//Nothing cached
		cached = 0;
	}

	inline void Align()
	{
		if (cached%8==0)
			return;
		
		if (cached>24)
			Put(32-cached,0);
		else if (cached>16)
			Put(24-cached,0);
		else if (cached>8)
			Put(16-cached,0);
		else
			Put(8-cached,0);
	}

	inline DWORD Put(BYTE n,DWORD v)
	{
		
		if (!n) 
		{
			//Nothing to do
			return v;
		} else if (n+cached>32) {
			BYTE a = 32-cached;
			BYTE b =  n-a;
			//Check if cache is not full
			if (a)
				//Add remaining to cache
				cache = (cache << a) | ((v>>b) & (0xFFFFFFFF>>cached));
			//Set cached bytes
			cached = 32;
			//Flush into memory
			FlushCache();
			//Set new cache
			cache = v & (0xFFFFFFFF>>(32-b));
			//Increase cached
			cached = b;
		} else {
			//Add to cache
			cache = (cache << n) | (v & (0xFFFFFFFF>>(32-n)));
			//Increase cached
			cached += n;
		}
		//If it is last
		if (cached==bufferSize*8)
			//Flush it
			FlushCache();
		
		return v;
	}
	
	inline QWORD Left()
	{
		return QWORD(size - bufferLen) * 8 - cached;
	}

	inline DWORD Put(BYTE n,BitReader &reader)
	{
		return Put(n,reader.Get(n));
	}
	
	inline bool Error()
	{
		//We won't use exceptions, so we need to signal errors somehow
		return error;
	}
	
	inline bool WriteNonSymmetric(uint32_t num,uint32_t val) 
	{
  		if (num == 1)
			// When there is only one possible value, it requires zero bits to store it.
			return true;
		
		size_t count = CountBits(num);
		uint32_t numbits = (1u << count) - num;

		if (val < numbits)
		{
			if (Left()<count-1)
				return false;
			Put(count - 1, val);
		} else {
			if (Left()<count)
				return false;
			Put(count, val + numbits);
		}
		return true;
	}

private:
	BYTE* data;
	DWORD size;
	BYTE* buffer;
	DWORD bufferLen;
	DWORD bufferSize;
	DWORD cache;
	BYTE  cached;
	bool  error;
};

#endif	/* BITWRITTER_H */
