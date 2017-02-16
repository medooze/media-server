/* 
 * File:   bitstream.h
 * Author: Sergio
 *
 * Created on 9 de diciembre de 2010, 8:48
 */

#ifndef _BITSTREAM_H_
#define	_BITSTREAM_H_
#include "config.h"
#include "tools.h"
#include <stdexcept>

class BitReader
{
public:
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
	inline DWORD Get(DWORD n)
	{
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
		} else
			ret = GetCached(n);
		//Debug("Readed %d:\n",n);
		//BitDump(ret,n);
		return ret;
	}

	inline bool Check(int n,DWORD val)
	{
		return Get(n)==val;
	}

	inline void Skip(DWORD n)
	{
		if (n>cached)
		{
			//Get what is left to skip
			BYTE a = n-cached;
			//Cache next
			Cache();
			//Skip cache
			SkipCached(a);
		} else
			//Skip cache
			SkipCached(n);
	}

	inline QWORD Left()
	{
		return cached+bufferLen*8;
	}

	inline DWORD Peek(DWORD n)
	{
		DWORD ret = 0;
		if (n>cached)
		{
			//What we have to read next
			BYTE a = n-cached;
			//Get remaining in the cache
			ret = cache >> (32-n);
			//Get the remaining
			ret =  ret | (PeekNextCached() >> (32-a));
		} else
			ret = cache >> (32-n);
		//Debug("Peeked %d:\n",n);
		//BitDump(ret,n);
		return ret;
	}

	inline DWORD GetPos()
	{
		return bufferPos*8-cached;
	}
public:
	inline DWORD Cache()
	{
		//Check left buffer
		if (bufferLen>3)
		{
			//Update cache
			cache = get4(buffer,0);
			//Update bit count
			cached = 32;
			//Increase pointer
			buffer += 4;
			bufferPos += 4;
			//Decrease length
			bufferLen -= 4;
		} else if(bufferLen==3) {
			//Update cache
			cache = get3(buffer,0)<<8;
			//Update bit count
			cached = 24;
			//Increase pointer
			buffer += 3;
			bufferPos += 3;
			//Decrease length
			bufferLen -= 3;
		} else if (bufferLen==2) {
			//Update cache
			cache = get2(buffer,0)<<16;
			//Update bit count
			cached = 16;
			//Increase pointer
			buffer += 2;
			bufferPos += 2;
			//Decrease length
			bufferLen -= 2;
		} else if (bufferLen==1) {
			//Update cache
			cache  = get1(buffer,0)<<24;
			//Update bit count
			cached = 8;
			//Increase pointer
			buffer++;
			bufferPos++;
			//Decrease length
			bufferLen--;
		} else {
			//We can't use exceptions so set error flag
			error = true;
			//Exit
			return 0;
		}
			

		//Debug("Reading int cache");
		//BitDump(cache,cached);

		//return number of bits
		return cached;
	}

	inline DWORD PeekNextCached()
	{
		//Check left buffer
		if (bufferLen>3)
		{
			//return  cached
			return get4(buffer,0);
		} else if(bufferLen==3) {
			//return  cached
			return get3(buffer,0)<<8;
		} else if (bufferLen==2) {
			//return  cached
			return get2(buffer,0)<<16;
		} else if (bufferLen==1) {
			//return  cached
			return get1(buffer,0)<<24;
		} else {
			//We can't use exceptions so set error flag
			error = true;
			//Exit
			return 0;
		}
	}


	inline DWORD SkipCached(DWORD n)
	{
		//Move
		cache = cache << n;
		//Update cached bytes
		cached -= n;

	}

	inline DWORD GetCached(DWORD n)
	{
		//Get bits
		DWORD ret = cache >> (32-n);
		//Skip thos bits
		SkipCached(n);
		//Return bits
		return ret;
	}
	
	inline bool Error()
	{
		//We won't use exceptions, so we need to signal errors somehow
		return error;
	}

private:
	const BYTE* buffer;
	DWORD bufferLen;
	DWORD bufferPos;
	DWORD cache;
	BYTE  cached;
	bool  error;
};




class BitWritter{
public:
	BitWritter(BYTE *data,DWORD size)
	{
		//Store pointers
		this->data = data;
		this->size = size;
		//And reset to init values
		Reset();
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
		if (n+cached>32)
		{
			BYTE a = 32-cached;
			BYTE b =  n-a;
			//Add to cache
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

	inline DWORD Put(BYTE n,BitReader &reader)
	{
		return Put(n,reader.Get(n));
	}
	
	inline bool Error()
	{
		//We won't use exceptions, so we need to signal errors somehow
		return error;
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

template<typename T>
class VLCDecoder
{
public:
	inline void AddValue(DWORD code,BYTE len,T value)
	{
		BYTE aux[4];
		//Create writter
		BitWritter writter(aux,4);
		//Write data
		writter.Put(len,code);
		//Flush to memory
		writter.Flush();
		//Init the bit reader with the code
		BitReader reader(aux,len);
		//Start from the parent node
		Node *n=&table;
		//Iterate the tree
		for(BYTE i=0;i<len;i++)
		{
			//Get bit
			DWORD child = reader.Get(1);
			//chek not empty
			if(!n->childs[child])
				//Create it
				n->childs[child] = new Node();
			//Get next node
			n = n->childs[child];
		}
		//Set the value
		n->value = value;
	}

	inline T Decode(BitReader &reader)
	{
		//Debug("VLC [");
		//Start from the parent node
		Node *n=&table;
		//Iterate the tree
		while(!n->value)
		{
			BYTE v = reader.Get(1);
			//Debug("%d",v);
			//Get next node
			n = n->childs[v];
			//check valid node
			if (!n)
				//No value found, erroneus code
				return NULL;
		}
		//Debug("]\n");
		//Return found value
		return n->value;
	}
private:
	struct Node
	{
		Node()
		{
			childs[0] = NULL;
			childs[1] = NULL;
			value = NULL;
		}
		Node* childs[2];
		T value;
	};

	Node	table;
};

class ExpGolombDecoder
{
public:
	static inline  DWORD Decode(BitReader &reader)
	{
		//No len yet
		DWORD len = 0;
		//Count zeros
		while (!reader.Get(1))
			//Increase len
			++len;
		//Get the exp
		DWORD value = reader.Get(len);
		//Calc value
		return (1<<len)-1+value;
	}

	static inline int DecodeSE(BitReader &reader)
	{
		//Get code num
		DWORD codeNum = Decode(reader);
		//Conver to signed
		return codeNum & 0x01 ? codeNum>>1 : -(codeNum>>1);
	}
};

#endif	/* BITSTREAM_H */
