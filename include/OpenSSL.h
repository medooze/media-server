#ifndef MCU_OPENSSL_H_
#define MCU_OPENSSL_H_


#include <pthread.h>
#include <openssl/opensslconf.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


class OpenSSL
{
public:
	static bool ClassInit();

private:
	static bool SetThreadSafe();
	static void SetThreadId(CRYPTO_THREADID* thread_id);
	static void LockingFunction(int mode, int n, const char *file, int line);

private:
	static pthread_mutex_t* mutexArray;
};

template <typename T, void (*function)(T*)>
struct SSLFunctionDeleter {
  void operator()(T* pointer) const { function(pointer); }
};

using SSLCtxDeleter = SSLFunctionDeleter<SSL_CTX, SSL_CTX_free>;
using SSLDeleter = SSLFunctionDeleter<SSL, SSL_free>;

#endif
