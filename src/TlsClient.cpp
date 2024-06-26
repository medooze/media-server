#include "TlsClient.h"
#include "log.h"

namespace {
	
void LogCertificateInfo(int preverify, X509_STORE_CTX* ctx)
{
	auto cert = X509_STORE_CTX_get_current_cert(ctx);
	char name[256];
	X509_NAME_oneline(X509_get_subject_name(cert), name, sizeof(name));
	
	auto err = X509_STORE_CTX_get_error(ctx);
	auto depth = X509_STORE_CTX_get_error_depth(ctx);
	
	Log("-TlsClient::initialize() SSL certificate subject: %s preverify: %d error: %s depth: %d\n", 
		name, preverify, X509_verify_cert_error_string(err), depth);
}
	
}


TlsClient::TlsClient(bool allowAllCertificates) :
	allowAllCertificates(allowAllCertificates)
{
}

bool TlsClient::initialize(const char* hostname)
{
	ctx = SSL_CTX_new(TLS_method());
	if (!ctx)
	{
		Error("-TlsClient::initialize() Failed to ceate SSL context\n");
		return false;
	}
	
	SSL_CTX_set_default_verify_dir(ctx);
	
	if (allowAllCertificates)
	{
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, [](int preverify, X509_STORE_CTX* ctx) -> int {
			LogCertificateInfo(preverify, ctx);
			// Allow all certificates
			return 1;
		});
	}
	else
	{
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, [](int preverify, X509_STORE_CTX* ctx) -> int {
			LogCertificateInfo(preverify, ctx);
			return preverify;
		});
	}
	
	SSL_CTX_set_options(ctx, SSL_OP_ALL);

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
		SSL_set_tlsext_host_name(ssl, hostname);
	}
	
	return true;
}

TlsClient::TlsClientError TlsClient::decrypt(const uint8_t* data, size_t size)
{
	auto len = BIO_write(rbio, data, size);
	if (len <= 0)
	{
		Error("-TlsClient::decrypt() Failed to BIO_write\n");
		return TlsClientError::Failed;
	}
	
	if (!SSL_is_init_finished(ssl))
	{
		Log("-TlsClient::decrypt() ssl not initialised\n");
		initialised = false;
		
		if (handshake() == SslStatus::Failed)
		{
			Error("-TlsClient::decrypt() Failed to handshake\n");
			return TlsClientError::HandshakeFailed;
		}
		
		if (!SSL_is_init_finished(ssl))
		{
			Error("-TlsClient::decrypt() Pending init\n");
			return TlsClientError::Pending;
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
	
	auto status = getSslStatus(bytes);
	// This may happen when ssl renegotiation is needed
	if (status == SslStatus::Pending)
	{
		return readBioEncrypted() ? TlsClientError::NoError : TlsClientError::Failed;
	}
	
	return TlsClientError::NoError;
}

TlsClient::TlsClientError TlsClient::encrypt(const uint8_t* data, size_t size)
{
	if (size == 0) return TlsClientError::NoError;
	
	if (!SSL_is_init_finished(ssl))
	{
		initialised = false;
		return TlsClientError::Pending;
	}
	
	auto len = SSL_write(ssl, data, size);
	if (len > 0)
	{
		return readBioEncrypted() ? TlsClientError::NoError : TlsClientError::Failed;
	}
	else
	{
		return TlsClientError::Failed;
	}
}

void TlsClient::shutdown()
{
	initialised = false;
	
	encrypted.clear();
	decrypted.clear();
	
	SSL_free(ssl);
	SSL_CTX_free(ctx);
}

TlsClient::SslStatus TlsClient::getSslStatus(int returnCode)
{
	switch (SSL_get_error(ssl, returnCode))
	{
		case SSL_ERROR_NONE:
			return SslStatus::OK;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_READ:
			return SslStatus::Pending;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
		default:
			return SslStatus::Failed;
	}
}

TlsClient::SslStatus TlsClient::handshake()
{	
	auto ret = SSL_do_handshake(ssl);
	
	auto TlsError = getSslStatus(ret);
	if (TlsError == SslStatus::Pending)
	{
		return readBioEncrypted() ? SslStatus::OK : SslStatus::Failed;
	}
	
	return TlsError;
}

bool TlsClient::readBioEncrypted()
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
	
	return true;
}

void TlsClient::queueEncryptedData(const uint8_t* data, size_t size)
{
	encrypted.emplace_back(data, data + size);
}

void TlsClient::queueDecryptedData(const uint8_t* data, size_t size)
{
	decrypted.emplace_back(data, data + size);
}
