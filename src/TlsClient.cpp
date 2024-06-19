#include "TlsClient.h"
#include "log.h"

#include <cassert>

TlsClient::TlsClient()
{
	/* SSL library initialisation */
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	
	assert(OPENSSL_VERSION_MAJOR >= 3);
	ERR_load_crypto_strings();	
}

bool TlsClient::initialize(int fd, const char* hostname)
{
	/* create the SSL server context */
	ctx = SSL_CTX_new(TLS_method());
	if (!ctx)
	{
		Error("SSL_CTX_new()");
		return false;
	}

	/* Recommended to avoid SSLv2 & SSLv3 */
	SSL_CTX_set_options(ctx, SSL_OP_NO_QUERY_MTU);

	// Init client
	fd = fd;

	rbio = BIO_new(BIO_s_mem());
	wbio = BIO_new(BIO_s_mem());
	
	ssl = SSL_new(ctx);
	if (!ssl)
	{
		Error("SSL_new()");
		return false;
	}

	SSL_set_connect_state(ssl); /* ssl client mode */

	SSL_set_bio(ssl, rbio, wbio);
	
	if (hostname)
	{
		SSL_set_tlsext_host_name(ssl, hostname); // TLS SNI
	}
	
	if (handshake() == SslError::Failed)
	{
		return false;
	}
	
	return true;
}

bool TlsClient::decrypt(const uint8_t* data, size_t size)
{
	Log("Decrypt: %llu\n", size);
	
	auto len = BIO_write(rbio, data, size);
	if (len <= 0)
	{
		return false;
	}
	
	if (!SSL_is_init_finished(ssl))
	{
		if (handshake() == SslError::Failed) return false;
		
		if (!SSL_is_init_finished(ssl))
		{
			return true;
		}

		return true;
	}
	else
	{
		initialised = true;
	}
	
	while (true)
	{
		auto bytes = SSL_read(ssl, decryptCache, sizeof(decryptCache));
		if (bytes > 0)
		{
			queueDecryptedData(decryptCache, bytes);
		}
		else
		{
			break;
		}
	}
	
	return true;
}

bool TlsClient::encrypt(const uint8_t* data, size_t size)
{
	if (!SSL_is_init_finished(ssl))
	{
		return false;
	}
	
	auto len = SSL_write(ssl, data, size);
	if (len > 0)
	{
		int bytes = 0;
		do {
			bytes = BIO_read(wbio, encryptCache, sizeof(encryptCache));
			if (bytes > 0)
			{
				queueEncryptedData(encryptCache, bytes);
			}
			else if (!BIO_should_retry(wbio))
			{
				return false;
			}
		} while (bytes > 0);
	}
	else
	{
		return false;
	}
	
	return true;
}

void TlsClient::shutdown()
{
	SSL_free(ssl);
	SSL_CTX_free(ctx);
}

TlsClient::SslError TlsClient::getSslStatus(int returnCode)
{
	switch (SSL_get_error(ssl, returnCode))
	{
		case SSL_ERROR_NONE:
			return SslError::None;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_READ:
			return SslError::Pending;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
		default:
			return SslError::Failed;
	}
}

TlsClient::SslError TlsClient::handshake()
{	
	auto ret = SSL_do_handshake(ssl);
	
	auto sslError = getSslStatus(ret);
	if (sslError == SslError::Pending)
	{
		int bytes = 0;
		do {
			bytes = BIO_read(wbio, encryptCache, sizeof(encryptCache));
			if (bytes > 0)
			{
				queueEncryptedData(encryptCache, bytes);
			}
			else if (!BIO_should_retry(wbio))
			{
				return SslError::Failed;
			}
		} while (bytes > 0);
	}
	
	return sslError;
}

void TlsClient::queueEncryptedData(const uint8_t* data, size_t size)
{
	encrypted.emplace_back(data, data + size);
}

void TlsClient::queueDecryptedData(const uint8_t* data, size_t size)
{
	decrypted.emplace_back(data, data + size);
}