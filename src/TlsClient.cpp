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

bool TlsClient::initialize(const char* hostname)
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
	if (handshake() == Status::Failed)
	{
		return false;
	}
	
	return true;
}

TlsClient::Status TlsClient::decrypt(const uint8_t* data, size_t size)
{
	auto len = BIO_write(rbio, data, size);
	if (len <= 0)
	{
		Error("-TlsClient::decrypt() Failed to BIO_write\n");
		return TlsClient::Status::Failed;
	}
	
	if (!SSL_is_init_finished(ssl))
	{
		Log("-TlsClient::decrypt() ssl not initialised\n");
		
		if (handshake() == Status::Failed)
		{
			Error("-TlsClient::decrypt() Failed to handshake\n");
			return TlsClient::Status::Failed;
		}
		
		if (!SSL_is_init_finished(ssl))
		{
			Error("-TlsClient::decrypt() Pending init\n");
			return TlsClient::Status::Pending;
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
	
	return TlsClient::Status::OK;
}

TlsClient::Status TlsClient::encrypt(const uint8_t* data, size_t size)
{
	if (size == 0) return TlsClient::Status::OK;
	
	if (!SSL_is_init_finished(ssl))
	{
		return TlsClient::Status::Pending;
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
				return TlsClient::Status::Failed;
			}
		} while (bytes > 0);
	}
	else
	{
		return TlsClient::Status::Failed;
	}
	
	return TlsClient::Status::OK;
}

void TlsClient::shutdown()
{
	initialised = false;
	
	encrypted.clear();
	decrypted.clear();
	
	SSL_free(ssl);
	SSL_CTX_free(ctx);
}

TlsClient::Status TlsClient::getSslStatus(int returnCode)
{
	switch (SSL_get_error(ssl, returnCode))
	{
		case SSL_ERROR_NONE:
			return Status::OK;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_READ:
			return Status::Pending;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
		default:
			return Status::Failed;
	}
}

TlsClient::Status TlsClient::handshake()
{	
	auto ret = SSL_do_handshake(ssl);
	
	auto TlsError = getSslStatus(ret);
	if (TlsError == Status::Pending)
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
				return Status::Failed;
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
