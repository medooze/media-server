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
	
	if (handshake() == TlsError::Failed)
	{
		return false;
	}
	
	return true;
}

TlsClient::TlsError TlsClient::decrypt(const uint8_t* data, size_t size)
{
	Log("Decrypt: %llu\n", size);
	
	auto len = BIO_write(rbio, data, size);
	if (len <= 0)
	{
		Error("Failed to BIO_write\n");
		return TlsClient::TlsError::Failed;
	}
	
	if (!SSL_is_init_finished(ssl))
	{
		if (handshake() == TlsError::Failed)
		{
			Error("Failed to handshake\n");
			return TlsClient::TlsError::Failed;
		}
		
		if (!SSL_is_init_finished(ssl))
		{
			Error("Pending init\n");
			return TlsClient::TlsError::Pending;
		}
	}
	else if (!initialised)
	{
		Log("Tls client initialised\n");
		
		initialised = true;
	}
	
	for (auto bytes = SSL_read(ssl, decryptCache, sizeof(decryptCache)); bytes > 0;)
	{
		queueDecryptedData(decryptCache, bytes);
	}
	
	return TlsClient::TlsError::None;
}

TlsClient::TlsError TlsClient::encrypt(const uint8_t* data, size_t size)
{
	Log("encrypt: %llu\n", size);
	
	if (!SSL_is_init_finished(ssl))
	{
		return TlsClient::TlsError::Pending;
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
				return TlsClient::TlsError::Failed;
			}
		} while (bytes > 0);
	}
	else
	{
		return TlsClient::TlsError::Failed;
	}
	
	return TlsClient::TlsError::None;
}

void TlsClient::shutdown()
{
	SSL_free(ssl);
	SSL_CTX_free(ctx);
}

TlsClient::TlsError TlsClient::getSslStatus(int returnCode)
{
	switch (SSL_get_error(ssl, returnCode))
	{
		case SSL_ERROR_NONE:
			return TlsError::None;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_READ:
			return TlsError::Pending;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
		default:
			return TlsError::Failed;
	}
}

TlsClient::TlsError TlsClient::handshake()
{	
	auto ret = SSL_do_handshake(ssl);
	
	auto TlsError = getSslStatus(ret);
	if (TlsError == TlsError::Pending)
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
				return TlsError::Failed;
			}
		} while (bytes > 0);
	}
	
	return TlsError;
}

void TlsClient::queueEncryptedData(const uint8_t* data, size_t size)
{
	encrypted.emplace_back(data, data + size);
}

void TlsClient::queueDecryptedData(const uint8_t* data, size_t size)
{
	decrypted.emplace_back(data, data + size);
}