#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>
#include "config.h"
#include "tools.h"

class Logger
{
public:
        static Logger& getInstance()
        {
            static Logger   instance;
            return instance;
        }
	
	static bool IsUltraDebugEnabled()
	{
		return getInstance().ultradebug;
	}
	
	static bool IsDebugEnabled()
	{
		return getInstance().debug;
	}

	static bool EnableDebug(bool debug)
	{
		return getInstance().debug = debug;
	}
	
	static bool EnableUltraDebug(bool ultradebug)
	{
		if (ultradebug) EnableDebug(ultradebug);
		return getInstance().ultradebug = ultradebug;
	}

	inline int Log(const char *msg, ...)
	{
		return 1;
	}

	inline int Error(const char *msg, ...)
	{
		return 0;
	}
protected:
	bool debug;
	bool ultradebug;
private:
        Logger()
	{
		debug = false;
		ultradebug = false;
	}
        // Dont forget to declare these two. You want to make sure they
        // are unaccessable otherwise you may accidently get copies of
        // your singelton appearing.
        Logger(Logger const&);			// Don't Implement
        void operator=(Logger const&);		// Don't implement
};

inline int Log(const char *msg, ...)
{
	struct timeval tv;
	va_list ap;
	gettimeofday(&tv,NULL);
	printf("[0x%lx][%.10ld.%.3ld][LOG]", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000);
	va_start(ap, msg);
	vprintf(msg, ap);
	va_end(ap);
	fflush(stdout);
	return 1;
}

inline int Log2(const char* prefix,const char *msg, ...)
{
	struct timeval tv;
	va_list ap;
	gettimeofday(&tv,NULL);
	printf("[0x%lx][%.10ld.%.3ld][LOG]%s ", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000,prefix);
	va_start(ap, msg);
	vprintf(msg, ap);
	va_end(ap);
	fflush(stdout);
	return 1;
}

inline int UltraDebug(const char *msg, ...)
{
	if (Logger::IsUltraDebugEnabled())
	{
		struct timeval tv;
		va_list ap;
		gettimeofday(&tv,NULL);
		printf("[0x%lx][%.10ld.%.3ld][DBG]", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000);
		va_start(ap, msg);
		vprintf(msg, ap);
		va_end(ap);
		fflush(stdout);
	}
	return 1;
}

inline int Debug(const char *msg, ...)
{
	if (Logger::IsDebugEnabled())
	{
		struct timeval tv;
		va_list ap;
		gettimeofday(&tv,NULL);
		printf("[0x%lx][%.10ld.%.3ld][DBG]", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000);
		va_start(ap, msg);
		vprintf(msg, ap);
		va_end(ap);
		fflush(stdout);
	}
	return 1;
}

inline int Error(const char *msg, ...)
{
	struct timeval tv;
	va_list ap;
	gettimeofday(&tv,NULL);
	printf("[0x%lx][%.10ld.%.3ld][ERR]", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000);
	va_start(ap, msg);
	vprintf(msg, ap);
	va_end(ap);
	return 0;
}

inline void BitDump(DWORD val,BYTE n)
{
	char line1[136];
	char line2[136];
	int i=0;
	if (n>24)
	{
		sprintf(line1,"0x%.2x     0x%.2x     0x%.2x     0x%.2x     ",(BYTE)(val>>24),(BYTE)(val>>16),(BYTE)(val>>8),(BYTE)(val));
		i+= BitPrint(line2,(BYTE)(val>>24),n-24);
		i+= BitPrint(line2+i,(BYTE)(val>>16),8);
		i+= BitPrint(line2+i,(BYTE)(val>>8),8);
		i+= BitPrint(line2+i,(BYTE)(val),8);
	} else if (n>16) {
		sprintf(line1,"0x%.2x     0x%.2x     0x%.2x     ",(BYTE)(val>>16),(BYTE)(val>>8),(BYTE)(val));
		i+= BitPrint(line2+i,(BYTE)(val>>16),n-16);
		i+= BitPrint(line2+i,(BYTE)(val>>8),8);
		i+= BitPrint(line2+i,(BYTE)(val),8);
	} else if (n>8) {
		sprintf(line1,"0x%.2x     0x%.2x     ",(BYTE)(val>>8),(BYTE)(val));
		i+= BitPrint(line2,(BYTE)(val>>8),n-8);
		i+= BitPrint(line2+i,(BYTE)(val),8);
	} else {
		sprintf(line1,"0x%.2x     ",(BYTE)(val));
		BitPrint(line2,(BYTE)(val),n);
	}
	Debug("Dumping 0x%.4x:%d\n\t%s\n\t%s\n",val,n,line1,line2);
}

inline void BitDump(WORD val)
{
	BitDump(val,16);
}

inline void BitDump(DWORD val)
{
	BitDump(val,32);
}

inline void BitDump(QWORD val)
{
	BitDump(val>>32,32);
	BitDump(val,32);
}

inline void Dump(const BYTE *data,DWORD size)
{
	int i;

	for(i=0;i<(size/8);i++)
		Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   %c%c%c%c%c%c%c%c]\n",8*i,data[8*i],data[8*i+1],data[8*i+2],data[8*i+3],data[8*i+4],data[8*i+5],data[8*i+6],data[8*i+7],PC(data[8*i]),PC(data[8*i+1]),PC(data[8*i+2]),PC(data[8*i+3]),PC(data[8*i+4]),PC(data[8*i+5]),PC(data[8*i+6]),PC(data[8*i+7]));
	switch(size%8)
	{
		case 1:
			Debug("[%.4x] [0x%.2x                                                    %c       ]\n",size-1,data[size-1],PC(data[size-1]));
			break;
		case 2:
			Debug("[%.4x] [0x%.2x   0x%.2x                                             %c%c      ]\n",size-2,data[size-2],data[size-1],PC(data[size-2]),PC(data[size-1]));
			break;
		case 3:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x                                      %c%c%c     ]\n",size-3,data[size-3],data[size-2],data[size-1],PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 4:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x                               %c%c%c%c    ]\n",size-4,data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 5:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x                        %c%c%c%c%c   ]\n",size-5,data[size-5],data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-5]),PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 6:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x                 %c%c%c%c%c%c  ]\n",size-6,data[size-6],data[size-5],data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-6]),PC(data[size-5]),PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 7:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x          %c%c%c%c%c%c%c ]\n",size-7,data[size-7],data[size-6],data[size-5],data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-7]),PC(data[size-6]),PC(data[size-5]),PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
	}
}




#endif
