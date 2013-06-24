#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include "version.h"
#define QCIF	0	// 176  x 144
#define CIF	1	// 352  x 288
#define VGA	2	// 640  x 480
#define PAL	3	// 768  x 576
#define HVGA	4	// 320  x 240
#define QVGA	5	// 160  x 120
#define HD720P	6	// 1280 x 720
#define WQVGA	7	// 400  x 240
#define w448P   8	// 768  x 448
#define sd448P  9	// 576  x 448
#define w288P   10	// 512  x 288
#define w576    11	// 1024 x 576
#define FOURCIF 12	// 704  x 576
#define FOURSIF 13	// 704  x 576
#define XGA     14	// 1024 x 768


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

inline DWORD GetWidth(DWORD size)
{
	//Depending on size
	switch(size)
	{
		case QCIF:	return 176;
		case CIF:	return 352;
		case PAL:	return 768;
		case QVGA:	return 160;
		case HVGA:	return 320;
		case VGA:	return 640;
		case HD720P:	return 1280;
		case WQVGA:	return 400;
		case w448P:	return 768;
		case sd448P:	return 576;
		case w288P:	return 512;
		case w576:	return 1024;
		case FOURCIF:	return 704;
		case FOURSIF:	return 704;
		case XGA:	return 1024;
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
		case PAL:	return 576;
		case QVGA:	return 120;
		case HVGA:	return 240;
		case VGA:	return 480;
		case HD720P:	return 720;
		case WQVGA:	return 240;
		case w448P:	return 448;
		case sd448P:	return 448;
		case w288P:	return 288;
		case w576:	return 576;
		case FOURCIF:	return 576;
		case FOURSIF:	return 576;
		case XGA:	return 768;
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
#endif
