#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <malloc.h>
#include "version.h"

#define	QCIF	0	// 176  x 144 	AR:	1,222222222
#define	CIF	1	// 352  x 288	AR:	1,222222222
#define	VGA	2	// 640  x 480	AR:	1,333333333
#define	PAL	3	// 768  x 576	AR:	1,333333333
#define	HVGA	4	// 480  x 320	AR:	1,5
#define	QVGA	5	// 320  x 240	AR:	1,333333333
#define	HD720P	6	// 1280 x 720	AR:	1,777777778
#define	WQVGA	7	// 400  x 240	AR:	1,666666667
#define	W448P	8	// 768  x 448	AR:	1,714285714
#define	SD448P	9	// 576  x 448	AR:	1,285714286
#define	W288P	10	// 512  x 288	AR:	1,777777778
#define	W576	11	// 1024 x 576	AR:	1,777777778
#define	FOURCIF	12	// 704  x 576	AR:	1,222222222
#define	FOURSIF	13	// 704  x 480	AR:	1,466666667
#define	XGA	14	// 1024 x 768	AR:	1,333333333
#define	WVGA	15	// 800  x 480	AR:	1,666666667
#define	DCIF	16	// 528  x 384	AR:	1,375
#define	SIF	17	// 352  x 240	AR:	1,466666667
#define	QSIF	18	// 176  x 120	AR:	1,466666667
#define	SD480P	19	// 480  x 360	AR:	1,333333333
#define	SQCIF	20	// 128  x 96	AR:	1,333333333
#define	SCIF	21	// 256  x 192	AR:	1,333333333



#define FD_INVALID	(int)-1
#define MTU		1500
#define RTPPAYLOADSIZE	1350

#define QWORD		uint64_t
#define DWORD		uint32_t
#define WORD		uint16_t
#define SWORD		int16_t
#define BYTE		uint8_t
#define SBYTE		char
#define ALIGNTO32(x)	(BYTE*)(((((QWORD)x)+31)>>5)<<5);
#define SIZE2MUL(x)	((x)&0xFFFFFFFE)
#define SIZE4MUL(x)	((x)&0xFFFFFFFC)
#define MAXKBITS 	300

#define ALIGNEDTO32        __attribute__ ((aligned (32)))
#define ZEROALIGNEDTO32    __attribute__ ((aligned (32))) = {0}

inline DWORD GetWidth(DWORD size)
{
	//Depending on size
	switch(size)
	{
		case QCIF:	return 176;
		case CIF:	return 352;
		case VGA:	return 640;
		case PAL:	return 768;
		case HVGA:	return 480;
		case QVGA:	return 320;
		case HD720P:	return 1280;
		case WQVGA:	return 400;
		case W448P:	return 768;
		case SD448P:	return 576;
		case W288P:	return 512;
		case W576:	return 1024;
		case FOURCIF:	return 704;
		case FOURSIF:	return 704;
		case XGA:	return 1024;
		case WVGA:	return 800;
		case DCIF:	return 528;
		case SIF:	return 352;
		case QSIF:	return 176;
		case SD480P:	return 480;
		case SQCIF:	return 128;
		case SCIF:	return 256;
	}
	//Nothing
	return 0;
}

inline DWORD GetHeight(DWORD size)
{
	//Depending on size
	switch(size)
	{
		case QCIF:	return 144;
		case CIF:	return 288;
		case VGA:	return 480;
		case PAL:	return 576;
		case HVGA:	return 320;
		case QVGA:	return 240;
		case HD720P:	return 720;
		case WQVGA:	return 240;
		case W448P:	return 448;
		case SD448P:	return 448;
		case W288P:	return 288;
		case W576:	return 576;
		case FOURCIF:	return 576;
		case FOURSIF:	return 480;
		case XGA:	return 768;
		case WVGA:	return 480;
		case DCIF:	return 384;
		case SIF:	return 240;
		case QSIF:	return 120;
		case SD480P:	return 360;
		case SQCIF:	return 96;
		case SCIF:	return 192;
	}
	//Nothing
	return 0;
}

class Properties: public std::map<std::string,std::string>
{
public:
	bool HasProperty(const std::string key) const
	{
		return find(key)!=end();
	}

	void SetProperty(const char* key,const char* val)
	{
		insert(std::pair<std::string,std::string>(std::string(key),std::string(val)));
	}
	std::string GetProperty(const char* key,const std::string defaultValue) const
	{
		//Find item
		const_iterator it = find(std::string(key));
		//If not found
		if (it==end())
			//return default
			return defaultValue;
		//Return value
		return it->second;
	}
	
	std::string GetProperty(const std::string key,const std::string defaultValue) const
	{
		//Find item
		const_iterator it = find(key);
		//If not found
		if (it==end())
			//return default
			return defaultValue;
		//Return value
		return it->second;
	}

	const char* GetProperty(const char* key,char *defaultValue) const
	{
		//Find item
		const_iterator it = find(std::string(key));
		//If not found
		if (it==end())
			//return default
			return defaultValue;
		//Return value
		return it->second.c_str();
	}

	const char* GetProperty(const std::string key,char *defaultValue) const
	{
		//Find item
		const_iterator it = find(key);
		//If not found
		if (it==end())
			//return default
			return defaultValue;
		//Return value
		return it->second.c_str();
	}

	int GetProperty(const char* key,int defaultValue) const
	{
		//Find item
		const_iterator it = find(std::string(key));
		//If not found
		if (it==end())
			//return default
			return defaultValue;
		//Return value
		return atoi(it->second.c_str());
	}
	
	int GetProperty(const std::string key,int defaultValue) const
	{
		//Find item
		const_iterator it = find(key);
		//If not found
		if (it==end())
			//return default
			return defaultValue;
		//Return value
		return atoi(it->second.c_str());
	}
};
inline void* malloc32(size_t size)
{
	return memalign(32,size);
}
#endif
