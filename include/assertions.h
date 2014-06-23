#ifndef MCU_ASSERTIONS_H
#define	MCU_ASSERTIONS_H


/**
 * If the macro ENABLE_MCU_ASSERTIONS is set (by calling the compiler with -DENABLE_MCU_ASSERTIONS
 * or by uncommenting the line below) then custom assertions will be performed.
 */
#define ENABLE_MCU_ASSERTIONS


#include <stdio.h>
#include <stdlib.h>  // _Exit(), abort()
#include <assert.h>  // assert()


#ifdef ENABLE_MCU_ASSERTIONS
#define MCU_CLOSE(fd)  \
	({ do {  \
		if (fd>=0 && fd <= 2) {  \
			fprintf(stderr, "FATAL: close(fd) called with fd == %d, aborting!\n", fd);  \
			assert(fd > 2);  \
			abort();  \
		}  \
	} while(0); close(fd); })
#else
#define MCU_CLOSE(fd)  close(fd)
#endif


#ifdef ENABLE_MCU_ASSERTIONS
#define MCU_ASSERT(condition)  \
	if (! (condition)) {  \
		fprintf(stderr, "FATAL: assertion failed: '%s', aborting!\n", #condition);  \
		assert(condition);  \
		abort();  \
	}
#else
#define MCU_ASSERT(condition)
#endif


#endif
