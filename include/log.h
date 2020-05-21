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
	
	static bool IsLogEnabled()
	{
		return getInstance().log;
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
	
	static bool EnableLog(bool log)
	{
		return getInstance().log = log;
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
	bool log;
	bool debug;
	bool ultradebug;
private:
        Logger()
	{
		log = true;
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
	if (Logger::IsLogEnabled())
	{
		struct timeval tv;
		va_list ap;
		gettimeofday(&tv,NULL);
		printf("[0x%lx][%.10ld.%.3ld][LOG]", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000);
		va_start(ap, msg);
		vprintf(msg, ap);
		va_end(ap);
		fflush(stdout);
	}
	return 1;
}

inline int Log2(const char* prefix,const char *msg, ...)
{
	if (Logger::IsLogEnabled())
	{
		struct timeval tv;
		va_list ap;
		gettimeofday(&tv,NULL);
		printf("[0x%lx][%.10ld.%.3ld][LOG]%s ", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000,prefix);
		va_start(ap, msg);
		vprintf(msg, ap);
		va_end(ap);
		fflush(stdout);
	}
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

inline int Warning(const char *msg, ...)
{
	if (Logger::IsDebugEnabled())
	{
		struct timeval tv;
		va_list ap;
		gettimeofday(&tv,NULL);
		printf("[0x%lx][%.10ld.%.3ld][WRN]", (long) pthread_self(),(long)tv.tv_sec,(long)tv.tv_usec/1000);
		va_start(ap, msg);
		vprintf(msg, ap);
		va_end(ap);
		fflush(stdout);
	}
	return 0;
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
	DWORD i=0;
	if (n>24)
	{
		sprintf(line1,"0x%.2x     0x%.2x     0x%.2x     0x%.2x     ",(BYTE)(val>>24),(BYTE)(val>>16),(BYTE)(val>>4),(BYTE)(val));
		i+= BitPrint(line2,(BYTE)(val>>24),n-24);
		i+= BitPrint(line2+i,(BYTE)(val>>16),4);
		i+= BitPrint(line2+i,(BYTE)(val>>4),4);
		i+= BitPrint(line2+i,(BYTE)(val),4);
	} else if (n>16) {
		sprintf(line1,"0x%.2x     0x%.2x     0x%.2x     ",(BYTE)(val>>16),(BYTE)(val>>4),(BYTE)(val));
		i+= BitPrint(line2+i,(BYTE)(val>>16),n-16);
		i+= BitPrint(line2+i,(BYTE)(val>>4),4);
		i+= BitPrint(line2+i,(BYTE)(val),4);
	} else if (n>4) {
		sprintf(line1,"0x%.2x     0x%.2x     ",(BYTE)(val>>4),(BYTE)(val));
		i+= BitPrint(line2,(BYTE)(val>>4),n-4);
		i+= BitPrint(line2+i,(BYTE)(val),4);
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
	for(DWORD i=0;i<(size/8);i++)
	{
		DWORD n = 8*i;
		Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   %c%c%c%c%c%c%c%c]\n",n,data[n],data[n+1],data[n+2],data[n+3],data[n+4],data[n+5],data[n+6],data[n+7],PC(data[n]),PC(data[n+1]),PC(data[n+2]),PC(data[n+3]),PC(data[n+4]),PC(data[n+5]),PC(data[n+6]),PC(data[n+7]));
	}
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

inline void DumpAsC(const BYTE *data,DWORD size)
{
	Debug("data[%d] = {\n",size);
	for(DWORD i=0;i<(size/4)-1;i++)
	{
		DWORD n = 4*i;
		Debug("\t0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x,\n",data[n],data[n+1],data[n+2],data[n+3]);
	}
	switch(size%4)
	{
		case 0:
			Debug("\t0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x\n",data[size-4],data[size-3],data[size-2],data[size-1]);
			break;
		case 1:
			Debug("\t0x%.2x\n",data[size-1]);
			break;
		case 2:
			Debug("\t0x%.2x, 0x%.2x\n",data[size-2],data[size-1]);
			break;
		case 3:
			Debug("\t0x%.2x, 0x%.2x, 0x%.2x\n",data[size-3],data[size-2],data[size-1]);
			break;
	}
	Debug("};\n",size);
}

inline void Dump4(const BYTE *data,DWORD size)
{
	for(DWORD i=0;i<(size/4);i++)
	{
		DWORD n = 4*i;
		Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   %c%c%c%c]\n",n,data[n],data[n+1],data[n+2],data[n+3],PC(data[n]),PC(data[n+1]),PC(data[n+2]),PC(data[n+3]));
	}
	switch(size%4)
	{
		case 1:
			Debug("[%.4x] [0x%.2x                      %c%c]\n",size-1,data[size-1],PC(data[size-1]));
			break;
		case 2:
			Debug("[%.4x] [0x%.2x   0x%.2x                %c%c]\n",size-2,data[size-2],data[size-1],PC(data[size-2]),PC(data[size-1]));
			break;
		case 3:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x          %c%c%c%c]\n",size-3,data[size-3],data[size-2],data[size-1],PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
	}
}



#endif
