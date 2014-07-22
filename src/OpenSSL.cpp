#include "OpenSSL.h"
#include "log.h"
#include <cstdlib>  // malloc()


/* Static attributes. */

// This array will store all of the mutex available for OpenSSL.
pthread_mutex_t* OpenSSL::mutexArray = NULL;


/* Static methods. */

bool OpenSSL::ClassInit()
{
	// First initialize OpenSSL stuff.
	SSL_load_error_strings();
	SSL_library_init();

	// Then set the stuff to make OpenSSL thread-safe.
	if (! SetThreadSafe()) {
		::Error("-OpenSSL::ClassInit(): SetThreadSafe() failed\n");
		return false;
	}

	return true;
}


bool OpenSSL::SetThreadSafe()
{
	int err;

	mutexArray = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * CRYPTO_num_locks());
	if (! mutexArray) {
		::Error("-OpenSSL::SetThreadSafe(): allocation of mutexArray failed\n");
		return false;
	}

	for (int i=0; i<CRYPTO_num_locks(); i++) {
		err = pthread_mutex_init(&mutexArray[i], NULL);
		if (err) {
			::Error("-OpenSSL::SetThreadSafe(): pthread_mutex_init() failed with return code %d\n", err);
			return false;
		}
	}

	/* Set OpenSSL thread id callback. */
	// Deprecated in OpenSSL >= 1.0.0.
	// CRYPTO_set_id_callback(GetThreadId);
	// For OpenSSL >= 1.0.0.
	CRYPTO_THREADID_set_callback(SetThreadId);

	/* Set OpenSSL locking callback. */
	CRYPTO_set_locking_callback(LockingFunction);

	return true;
}


// unsigned long OpenSSL::GetThreadId()
// {
// 	::Debug("-OpenSSL::GetThreadId()\n");
// 	return (unsigned long)pthread_self();
// }


void OpenSSL::SetThreadId(CRYPTO_THREADID* thread_id)
{
	// ::Debug("-OpenSSL::SetThreadId() [thread_id: 0x%lx]\n", (long)thread_id);
	// Let's assume pthread_self() is an unsigned long (otherwise this won't compile anyway).
	CRYPTO_THREADID_set_numeric(thread_id, (unsigned long)pthread_self());
}


void OpenSSL::LockingFunction(int mode, int n, const char *file, int line)
{
	/**
	 * - mode:  bitfield describing what should be done with the lock:
	 *     CRYPTO_LOCK     0x01
	 *     CRYPTO_UNLOCK   0x02
	 *     CRYPTO_READ     0x04
	 *     CRYPTO_WRITE    0x08
	 * - n:  the number of the mutex.
	 * - file:  source file calling this function.
	 * - line:  line in the source file calling this function.
	 */

	// ::Debug("-OpenSSL::LockingFunction() [mode: LOCK:%u|UNLOCK:%u|READ:%u|WRITE:%u, mutex number: %d, file: %s, line: %d]\n", mode & CRYPTO_LOCK ?1:0, mode & CRYPTO_UNLOCK ?1:0, mode & CRYPTO_READ ?1:0, mode & CRYPTO_WRITE ?1:0, n, file, line);

	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&mutexArray[n]);
	else
		pthread_mutex_unlock(&mutexArray[n]);
}
