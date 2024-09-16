#ifndef BITREADER_H_
#define	BITREADER_H_

#include <stdexcept>
#include <cassert>

#include "log.h"
#include "config.h"
#include "tools.h"
#include "BufferReader.h"


template<class Reader>
class BitReaderBase
{
public:

	/***
	* BitReader() will read lazily from the BufferReader
	*/
	BitReaderBase(Reader& reader) : reader(reader)
	{
	}

	inline DWORD Get(DWORD n)
	{
		if (n > 32)
			throw std::invalid_argument("BitReader::Get() n > 32");

		//Debug(">BitReader::Get() n:%d cached:%d\n", n, cached);
		//BitDump(cache >> (32 - cached), cached);

		DWORD ret = 0;
			
		if (n>cached)
		{
			//What we have to read next
			BYTE a = n-cached;
			//Get remaining in the cache
			ret = GetCached(cached) << a;
			//Cache next
			Cache();
			//Get the remaining
			ret |= GetCached(a);
		} else if (n) {
			//Get from cache
			ret = GetCached(n);
		}
		//Debug("<BitReader::Get() Readed n:%d ret:%d cached:%d\n", n, ret, cached);
		//BitDump(ret,n);
		return ret;
	}

	inline bool Check(int n, DWORD val)
	{
		return Get(n)==val;
	}

	inline void Skip(DWORD n)
	{
		//Debug(">BitReader::Skip() skipping n:%d: cached:%d\n", n, cached);
		//BitDump(cache >> (32 - cached), cached);

		//If input is bigger than cache
		while (n>32)
		{
			//Consume what is in the cache
			n -= cached;
			//Skip cached
			SkipCached(cached);
			//Cache next
			Cache();
		}

		if (n>cached) 
		{
			//Get what is left to skip
			BYTE a = n - cached;
			//Skip cached
			SkipCached(cached);
			//Cache next
			Cache();
			//Skip cache
			SkipCached(a);
		} else if (n) {
			//Skip cache
			SkipCached(n);
		}
		//Debug("<BitReader::Skip() Skiped n:%d: cached:%d\n", n, cached);
	}

	inline DWORD Peek(DWORD n)
	{
		if (n > 32)
			throw std::invalid_argument("BitReader::Peek() n > 32");

		DWORD ret = 0;

		if (n>cached) 
		{
			//What we have to read next
			BYTE a = n-cached;
			//Get remaining in the cache
			ret = cache >> (32-cached);
			//Get the remaining
			ret =  ret | (PeekNextCached() >> (32-a));
		} else if (n) {
			//Get from cache
			ret = cache >> (32-n);
		}
		//Debug("-BitReader::Peek() Peeked %d:\n",n);
		//BitDump(ret,n);
		return ret;
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

	inline void Flush()
	{
		//Debug("-BitReader::Flush()\n");
		Align();
		FlushCache();
	}

	inline void FlushCache()
	{
		//Nothing cached
		cached = 0;
		cache = 0;
	}

	inline void Align()
	{
		//Debug(">Align() cache len:%u\n", reader.GetLeft());

		//Go to next byte
		Skip(cached % 8);

		//Debug("<BitReader::Align() cache len:%u\n", reader.GetLeft());

	}

private:
	inline DWORD Cache()
	{

		assert(cached==0);

		//Check left buffer
		if (reader.Assert(4))
		{
			//Update cache
			cache = reader.Peek4();
			//Update bit count
			cached = 32;
		} else if(reader.Assert(3)) {
			//Update cache
			cache = reader.Peek3() << 8;
			//Update bit count
			cached = 24;
		} else if (reader.Assert(2)) {
			//Update cache
			cache = reader.Peek2() << 16;
			//Update bit count
			cached = 16;
		} else if (reader.Assert(1)) {
			//Update cache
			cache  = reader.Peek1()<<24;
			//Update bit count
			cached = 8;
		} else {
			throw std::range_error("no more bytes to read from");
		}
			

		//Debug("-BitReader::Cache() Reading int cache %x:%d\n",cache,cached);
		//BitDump(cache>>(32-cached),cached);

		//return number of bits
		return cached;
	}

	inline DWORD PeekNextCached()
	{
		//Check left buffer
		if (reader.Assert(4))
		{
			//Peek next bytes
			return reader.Peek4();
		} else if(reader.Assert(3)) {
			//Peek next bytes
			return reader.Peek3() << 8;
		} else if (reader.Assert(2)) {
			//Peek next bytes
			return reader.Peek2() << 16;
		} else if (reader.Assert(1)) {
			//Peek next bytes
			return reader.Peek1() << 24;
		} else {
			throw std::range_error("no more bytes to read from");
		}
	}

	inline void SkipCached(DWORD n)
	{
		assert(n <= cached);

		//Debug(">BitReader::SkipCached() n:%d cache %x:%d\n", n, cache, cached);

		//Check length
		if (!n) return;

		//Calculate consumed bytes
		uint8_t consumed = cached / 8 - (cached - n) / 8;

		//Move reader
		reader.Skip(consumed);

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

		//Debug("<BitReader::SkipCached() cache:%x cached:%d consumed:%d\n",cache,cached,consumed);
		//BitDump(cache >> (32 - cached), cached);
	}

	inline DWORD GetCached(DWORD n)
	{
		assert(n <= cached);

		//Check length
		if (!n) return 0;
		//Get bits
		DWORD ret = cache >> (32-n);
		//Skip those bits
		SkipCached(n);

		//Debug("-BitReader::GetCached() Readed n:%d ret:%d cached:%d\n", n, ret, cached);
		//Return bits
		return ret;
	}
	
private:
	Reader& reader;
	DWORD cache	= 0;
	BYTE  cached	= 0;
};


using BitReader = BitReaderBase<BufferReader>;

#endif	/* BITREADER_H_ */
