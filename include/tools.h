#ifndef _TOOLS_H_
#define _TOOLS_H_

#include "config.h"
#include <cstddef>  // size_t
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <signal.h>
#include <climits>
#include <pthread.h>

int Log(const char *msg, ...);

/*************************************
* blocksignals
*       Bloquea todas las sygnals para esa thread
*************************************/
inline int blocksignals()
{
	sigset_t mask;

        //Close it
        sigemptyset(&mask);
	//Solo cogemos el control-c
	sigaddset(&mask,SIGINT);
	sigaddset(&mask,SIGUSR1);

	//y bloqueamos
	return pthread_sigmask(SIG_BLOCK,&mask,0);
}

/*************************************
* createPriorityThread
*       Crea un nuevo hilo y le asigna la prioridad dada
*************************************/
inline int createPriorityThread(pthread_t *thread, void *(*function)(void *), void *arg, int priority)
{
	//Creamos el thread
	if (pthread_create(thread,NULL,function,arg) != 0)
		return 0;

	//Log
	Log("-Created thread [%p]\n",thread);

	return 1;
	/*
	 * //Aumentamos la prioridad
	struct sched_param parametros;
	parametros.sched_priority = priority;
	if (pthread_setschedparam(*thread,SCHED_RR,&parametros) != 0)
		return 0;

	return 1;
	*/
}


/************************************
* msleep
*	Duerme la cantidad de micro segundos usando un select
*************************************/
inline int msleep(long msec)
{
	struct timeval tv;

	tv.tv_sec=(int)((float)msec/1E6);
	tv.tv_usec=msec-tv.tv_sec*1E6;
	return select(0,0,0,0,&tv);
}

inline QWORD getTime(const struct timeval& now)
{
	return (((QWORD)now.tv_sec)*1E6+now.tv_usec);
}
inline QWORD getTime()
{
	//Obtenemos ahora
	struct timeval now;
	gettimeofday(&now,0);

	//Ahora calculamos la diferencia
	return (((QWORD)now.tv_sec)*1E6+now.tv_usec);
}

inline QWORD getTimeDiff(QWORD time)
{
	return getTime()-time;
}

inline QWORD getTimeMS()
{
	//Obtenemos ahora
	struct timeval now;
	gettimeofday(&now,0);

	//Ahora calculamos la diferencia
	return (((QWORD)now.tv_sec)*1000+now.tv_usec/1000);
}

inline QWORD getTimeDiffMS(QWORD time)
{
	return getTimeMS() - time;
}

/*********************************
* getDifTime
*	Obtiene cuantos microsegundos ha pasado desde "antes"
*********************************/
inline QWORD getDifTime(struct timeval *before)
{
	//Obtenemos ahora
	struct timeval now;
	gettimeofday(&now,0);

	//Ahora calculamos la diferencia
	return (((QWORD)now.tv_sec)*1000000+now.tv_usec) - (((QWORD)before->tv_sec)*1000000+before->tv_usec);
}

/*********************************
* getUpdDifTime
*	Calcula la diferencia entre "antes" y "ahora" y actualiza "antes"
*********************************/
inline QWORD getUpdDifTime(struct timeval *before)
{
	//Ahora calculamos la diferencia
	QWORD dif = getDifTime(before);

	//Actualizamos before
	gettimeofday(before,0);

	return dif;
}

/*********************************
* setZeroTime
*	Set time val to 0
*********************************/
inline void setZeroTime(struct timeval *val)
{
        //Fill with zeros
	val->tv_sec = 0;
	val->tv_usec = 0;
}

/*********************************
* isZeroTime
*	Check if time val is 0
*********************************/
inline DWORD isZeroTime(struct timeval *val)
{
        //Check if it is zero
        return !(val->tv_sec & val->tv_usec);
}

inline void setZeroThread(pthread_t *pthread)
{
        //Fill with zeros
	memset((BYTE*)pthread,0,sizeof(pthread_t));
}

/*********************************
* isZeroTime
*	Check if time val is 0
*********************************/
inline DWORD isZeroThread(const pthread_t thread)
{
	pthread_t zero;
	//Set zero thread
	setZeroThread(&zero);
        //Check if it is zero
        return memcmp(&thread,&zero,sizeof(pthread_t))==0;
}

/*****************************************
 * calcAbsTimout
 *      Create timespec value to be used in timeout calls
 **************************************/
inline void calcAbsTimeout(struct timespec *ts,struct timeval *tp,DWORD timeout)
{
	ts->tv_sec  = tp->tv_sec + timeout/1000;
	ts->tv_nsec = tp->tv_usec * 1000 + (timeout%1000)*1000000;

	//Check overflowns
	if (ts->tv_nsec>=1000000000)
	{
		//Decrease
		ts->tv_nsec -= 1000000000;
		//Inc seconds
		ts->tv_sec++;
	}
}

/*****************************************
 * calcAbsTimout
 *      Create timespec value to be used in timeout calls
 **************************************/
inline void calcAbsTimeoutNS(struct timespec *ts,struct timeval *tp,QWORD timeout)
{
	ts->tv_sec  = tp->tv_sec + timeout/1000000;
	ts->tv_nsec = tp->tv_usec * 1000 + (timeout%1000000)*1000;

	//Check overflowns
	if (ts->tv_nsec>=1000000000)
	{
		//Decrease
		ts->tv_nsec -= 1000000000;
		//Inc seconds
		ts->tv_sec++;
	}
}

/*****************************************
 * calcTimout
 *      Create timespec value to be used in timeout calls
 **************************************/
inline void calcTimout(struct timespec *ts,DWORD timeout)
{
	struct timeval tp;

        //Calculate now
        gettimeofday(&tp, NULL);
        //Increase timeout
        calcAbsTimeout(ts,&tp,timeout);

}


inline void EmptyCatch(int){};
inline BYTE  get1(const BYTE *data,size_t i) { return data[i]; }
inline DWORD get2(const BYTE *data,size_t i) { return (DWORD)(data[i+1]) | ((DWORD)(data[i]))<<8; }
inline DWORD get3(const BYTE *data,size_t i) { return (DWORD)(data[i+2]) | ((DWORD)(data[i+1]))<<8 | ((DWORD)(data[i]))<<16; }
inline DWORD get4(const BYTE *data,size_t i) { return (DWORD)(data[i+3]) | ((DWORD)(data[i+2]))<<8 | ((DWORD)(data[i+1]))<<16 | ((DWORD)(data[i]))<<24; }
inline QWORD get8(const BYTE *data,size_t i) { return ((QWORD)get4(data,i))<<32 | get4(data,i+4);	}

inline DWORD get3Reversed(const BYTE *data,size_t i) { return (DWORD)(data[i]) | ((DWORD)(data[i+1]))<<8 | ((DWORD)(data[i+2]))<<16; }

inline DWORD getN(BYTE n, BYTE* data, size_t i)
{
	switch (n)
	{
		case 1:
			return get1(data,i);
		case 2:
			return get2(data,i);
		case 3:
			return get3(data,i);
		case 4:
			return get4(data,i);
	}
	return 0;
}

inline void set1(BYTE *data,size_t i,BYTE val)
{
	data[i] = val;
}
inline void set2(BYTE *data,size_t i,DWORD val)
{
	data[i+1] = (BYTE)(val);
	data[i]   = (BYTE)(val>>8);
}
inline void set3(BYTE *data,size_t i,DWORD val)
{
	data[i+2] = (BYTE)(val);
	data[i+1] = (BYTE)(val>>8);
	data[i]   = (BYTE)(val>>16);
}
inline void set4(BYTE *data,size_t i,DWORD val)
{
	data[i+3] = (BYTE)(val);
	data[i+2] = (BYTE)(val>>8);
	data[i+1] = (BYTE)(val>>16);
	data[i]   = (BYTE)(val>>24);
}

inline void set6(BYTE *data,size_t i,QWORD val)
{
	data[i+5] = (BYTE)(val);
	data[i+4] = (BYTE)(val>>8);
	data[i+3] = (BYTE)(val>>16);
	data[i+2] = (BYTE)(val>>24);
	data[i+1] = (BYTE)(val>>32);
	data[i]   = (BYTE)(val>>40);
}

inline void set8(BYTE *data,size_t i,QWORD val)
{
	data[i+7] = (BYTE)(val);
	data[i+6] = (BYTE)(val>>8);
	data[i+5] = (BYTE)(val>>16);
	data[i+4] = (BYTE)(val>>24);
	data[i+3] = (BYTE)(val>>32);
	data[i+2] = (BYTE)(val>>40);
	data[i+1] = (BYTE)(val>>48);
	data[i]   = (BYTE)(val>>56);
}

inline void setN(BYTE n, BYTE* data, size_t i, DWORD val)
{
	switch (n)
	{
		case 1:
			return set1(data,i,val);
		case 2:
			return set2(data,i,val);
		case 3:
			return set3(data,i,val);
		case 4:
			return set4(data,i,val);
	}
}

inline char PC(BYTE b)
{
	if (b>32&&b<128)
		return b;
	else
		return '.';
}


inline DWORD BitPrint(char* out,BYTE val,BYTE n)
{
	int j=0;
	if (n>8) n=8;
	for (int i=0;i<(8-n);i++)
		out[j++] = 'x';
	for (int i=(8-n);i<8;i++)
		if ((val>>(7-i)) & 1)
			out[j++] = '1';
		else
			out[j++] = '0';
	out[j++] = ' ';
	out[j] = 0;

	return j;
}

/* ---------------- private code */

#define BASE64_DEC_STEP(i) do { \
    bits = map2[in[i]]; \
    if (bits & 0x80) \
        goto out ## i; \
    v = i ? (v << 6) + bits : bits; \
} while(0)

#define AV_BASE64_SIZE(x)  (((x)+2) / 3 * 4 + 1)

inline int av_base64_decode(uint8_t *out, const char *in_str, int out_size)
{
	static const uint8_t map2[256] =
	{
	    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff,

	    0x3e, 0xff, 0xff, 0xff, 0x3f, 0x34, 0x35, 0x36,
	    0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff,
	    0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0x00, 0x01,
	    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	    0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
	    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1a, 0x1b,
	    0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
	    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
	    0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,

			      0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	};
    uint8_t *dst = out;
    uint8_t *end = out + out_size;
    // no sign extension
    const uint8_t *in = (const uint8_t *)in_str;
    unsigned bits = 0xff;
    unsigned v;

    while (end - dst > 3) {
        BASE64_DEC_STEP(0);
        BASE64_DEC_STEP(1);
        BASE64_DEC_STEP(2);
        BASE64_DEC_STEP(3);
	set4(dst,0,v << 8);
        dst += 3;
        in += 4;
    }
    if (end - dst) {
        BASE64_DEC_STEP(0);
        BASE64_DEC_STEP(1);
        BASE64_DEC_STEP(2);
        BASE64_DEC_STEP(3);
        *dst++ = v >> 16;
        if (end - dst)
            *dst++ = v >> 8;
        if (end - dst)
            *dst++ = v;
        in += 4;
    }
    while (1) {
        BASE64_DEC_STEP(0);
        in++;
        BASE64_DEC_STEP(0);
        in++;
        BASE64_DEC_STEP(0);
        in++;
        BASE64_DEC_STEP(0);
        in++;
    }

out3:
    *dst++ = v >> 10;
    v <<= 2;
out2:
    *dst++ = v >> 4;
out1:
out0:
    return bits & 1 ? -1 : dst - out;
}

inline char *av_base64_encode(char *out, DWORD out_size, const uint8_t *in, DWORD in_size)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *ret, *dst;
    DWORD i_bits = 0;
    int i_shift = 0;
    DWORD bytes_remaining = in_size;

    if (in_size >= UINT_MAX / 4 ||
        out_size < AV_BASE64_SIZE(in_size))
        return NULL;
    ret = dst = out;
    while (bytes_remaining > 3) {
        i_bits = get4(in,0);
        in += 3; bytes_remaining -= 3;
        *dst++ = b64[ i_bits>>26        ];
        *dst++ = b64[(i_bits>>20) & 0x3F];
        *dst++ = b64[(i_bits>>14) & 0x3F];
        *dst++ = b64[(i_bits>>8 ) & 0x3F];
    }
    i_bits = 0;
    while (bytes_remaining) {
        i_bits = (i_bits << 8) + *in++;
        bytes_remaining--;
        i_shift += 8;
    }
    while (i_shift > 0) {
        *dst++ = b64[(i_bits << 6 >> i_shift) & 0x3f];
        i_shift -= 6;
    }
    while ((dst - ret) & 3)
        *dst++ = '=';
    *dst = '\0';

    return ret;
}

inline DWORD pad32(DWORD size)
{
	//Alling to 32 bits
	if (size & 0x03)
		//Add padding
		return  (size & 0xFFFFFFFC)+4;
	else
		return size;
}

/**
 * Align memory to 4 bytes boundary and pad the skipped memory with zero.
 * 
 * @param data The data to be aligned
 * @param size The size of existing data
 * 
 * @return The size after aligned 
 */
inline uint32_t alignMemory4Bytes(uint8_t* data, uint32_t size)
{
	auto aligned = pad32(size);
	memset(data + size, 0, aligned - size);
	return aligned;
}

// Counts the number of bits used in the binary representation of val.

inline size_t CountBits(uint64_t val)
{
	size_t count = 0;
	while (val != 0)
	{
		count++;
		val >>= 1;
	}
	return count;
}
#endif
