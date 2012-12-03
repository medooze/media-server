#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdint.h>
#include "version.h"
#define QCIF	0
#define CIF	1
#define VGA	2
#define PAL	3
#define HVGA	4
#define QVGA	5
#define HD720P	6
#define WQVGA	7

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
		case PAL:	return 704;
		case QVGA:	return 160;
		case HVGA:	return 320;
		case VGA:	return 640;
		case HD720P:	return 1280;
		case WQVGA:	return 400;
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
	}
	//Nothing
	return 0;
}
#endif
