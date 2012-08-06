#ifndef _TOOLS_H_
#define _TOOLS_H_

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <pthread.h>


#define LIMIT(x) ((x)>0xffffff?0xff: ((x)<=0xffff?0:((x)>>16)))

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
	struct sched_param parametros;
	parametros.sched_priority = priority;

	//Creamos el thread
	if (pthread_create(thread,NULL,function,arg) != 0)
		return 0;

	return 1;
	/*
	 * //Aumentamos la prioridad
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

	tv.tv_sec=(int)((float)msec/1000000);
	tv.tv_usec=msec-tv.tv_sec*1000000;
	return select(0,0,0,0,&tv);
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
inline BYTE  get1(BYTE *data,BYTE i) { return data[i]; }
inline DWORD get2(BYTE *data,BYTE i) { return (DWORD)(data[i+1]) | ((DWORD)(data[i]))<<8; }
inline DWORD get3(BYTE *data,BYTE i) { return (DWORD)(data[i+2]) | ((DWORD)(data[i+1]))<<8 | ((DWORD)(data[i]))<<16; }
inline DWORD get4(BYTE *data,BYTE i) { return (DWORD)(data[i+3]) | ((DWORD)(data[i+2]))<<8 | ((DWORD)(data[i+1]))<<16 | ((DWORD)(data[i]))<<24; }

inline void set1(BYTE *data,BYTE i,BYTE val)
{
	data[i] = val;
}
inline void set2(BYTE *data,BYTE i,DWORD val)
{
	data[i+1] = (BYTE)(val);
	data[i]   = (BYTE)(val>>8);
}
inline void set3(BYTE *data,BYTE i,DWORD val)
{
	data[i+2] = (BYTE)(val);
	data[i+1] = (BYTE)(val>>8);
	data[i]   = (BYTE)(val>>16);
}
inline void set4(BYTE *data,BYTE i,DWORD val)
{
	data[i+3] = (BYTE)(val);
	data[i+2] = (BYTE)(val>>8);
	data[i+1] = (BYTE)(val>>16);
	data[i]   = (BYTE)(val>>24);
}
inline void set8(BYTE *data,BYTE i,QWORD val)
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

inline char PC(BYTE b)
{
	if (b>32&&b<128)
		return b;
	else
		return '.';
}


inline DWORD BitPrint(char* out,BYTE val,BYTE n)
{
	char aux[2];
	int j=0;

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


#endif

