#include "TlsClient.h"
#include "log.h"

#include <cassert>

TlsClient::TlsClient()
{
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
		Error("-TlsClient::initialize() Failed to ceate SSL context\n");
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
		Error("-TlsClient::initialize() Failed to ceate SSL\n");
		return false;
	}

	// Set client mode
	SSL_set_connect_state(ssl);

	SSL_set_bio(ssl, rbio, wbio);
	
	if (hostname)
	{
		SSL_set_tlsext_host_name(ssl, hostname); // TLS SNI
	}
	
	// Start handshake
	if (handshake() == TlsError::Failed)
	{
		return false;
	}
	
	return true;
}

TlsClient::TlsError TlsClient::decrypt(const uint8_t* data, size_t size)
{
	auto len = BIO_write(rbio, data, size);
	if (len <= 0)
	{
		Error("-TlsClient::decrypt() Failed to BIO_write\n");
		return TlsClient::TlsError::Failed;
	}
	
	if (!SSL_is_init_finished(ssl))
	{
		Log("-TlsClient::decrypt() ssl not initialised\n");
		
		if (handshake() == TlsError::Failed)
		{
			Error("-TlsClient::decrypt() Failed to handshake\n");
			return TlsClient::TlsError::Failed;
		}
		
		if (!SSL_is_init_finished(ssl))
		{
			Error("-TlsClient::decrypt() Pending init\n");
			return TlsClient::TlsError::Pending;
		}
	}
	
	if (!initialised)
	{
		Log("-TlsClient::decrypt() Tls client initialised\n");
		initialised = true;
	}
	
	int bytes = 0;
	do {
		bytes = SSL_read(ssl, decryptCache, sizeof(decryptCache));
		if (bytes > 0) 
		{
			queueDecryptedData(decryptCache, bytes);
		}
	} while (bytes > 0);
	
	return TlsClient::TlsError::None;
}

TlsClient::TlsError TlsClient::encrypt(const uint8_t* data, size_t size)
{
	if (size == 0) return TlsClient::TlsError::None;
	
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