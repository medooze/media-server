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

constexpr size_t InitialCircularQueueSize = 256;
}


TlsClient::TlsClient(bool allowAllCertificates) :
	encrypted(InitialCircularQueueSize, true),
	decrypted(InitialCircularQueueSize, true),
	allowAllCertificates(allowAllCertificates)
{
}

TlsClient::~TlsClient()
{
	Shutdown();
}

bool TlsClient::Initialize(const char* hostname)
{
	ctx = SSLCtxPointer(SSL_CTX_new(TLS_method()));
	if (!ctx)
	{
		Error("-TlsClient::initialize() Failed to ceate SSL context\n");
		return false;
	}
	
	SSL_CTX_set_default_verify_dir(ctx.get());
	
	if (allowAllCertificates)
	{
		SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, [](int preverify, X509_STORE_CTX* ctx) -> int {
			LogCertificateInfo(preverify, ctx);
			// Allow all certificates
			return 1;
		});
	}
	else
	{
		SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, [](int preverify, X509_STORE_CTX* ctx) -> int {
			LogCertificateInfo(preverify, ctx);
			return preverify;
		});
	}
	
	SSL_CTX_set_options(ctx.get(), SSL_OP_ALL);

	rbio = BIOPointer(BIO_new(BIO_s_mem()));
	wbio = BIOPointer(BIO_new(BIO_s_mem()));
	
	ssl = SSLPointer(SSL_new(ctx.get()));
	if (!ssl)
	{
		Error("-TlsClient::initialize() Failed to ceate SSL\n");
		return false;
	}

	// Set client mode
	SSL_set_connect_state(ssl.get());

	SSL_set_bio(ssl.get(), rbio.get(), wbio.get());
	
	if (hostname)
	{
		SSL_set_tlsext_host_name(ssl.get(), hostname);
	}
	
	return true;
}

TlsClient::TlsClientError TlsClient::Decrypt(const uint8_t* data, size_t size)
{
	auto len = BIO_write(rbio.get(), data, size);
	if (len <= 0)
	{
		Error("-TlsClient::Decrypt() Failed to BIO_write\n");
		return TlsClientError::Failed;
	}
	
	if (!SSL_is_init_finished(ssl.get()))
	{
		Log("-TlsClient::Decrypt() ssl not initialised\n");
		initialised = false;
		
		if (Handshake() == SslStatus::Failed)
		{
			Error("-TlsClient::Decrypt() Failed to handshake\n");
			return TlsClientError::HandshakeFailed;
		}
		
		if (!SSL_is_init_finished(ssl.get()))
		{
			Error("-TlsClient::Decrypt() Pending init\n");
			return TlsClientError::Pending;
		}
	}
	
	if (!initialised)
	{
		Log("-TlsClient::Decrypt() Tls client initialised\n");
		initialised = true;
	}
	
	int bytes = 0;
	do {
		bytes = SSL_read(ssl.get(), decryptCache, sizeof(decryptCache));
		if (bytes > 0) 
		{
			QueueDecryptedData(decryptCache, bytes);
		}
	} while (bytes > 0);
	
	auto status = GetSslStatus(bytes);
	// This may happen when ssl renegotiation is needed
	if (status == SslStatus::Pending)
	{
		return ReadBioEncrypted() ? TlsClientError::NoError : TlsClientError::Failed;
	}
	
	return TlsClientError::NoError;
}

TlsClient::TlsClientError TlsClient::Encrypt(const uint8_t* data, size_t size)
{
	if (size == 0) return TlsClientError::NoError;
	
	if (!SSL_is_init_finished(ssl.get()))
	{
		initialised = false;
		return TlsClientError::Pending;
	}
	
	auto len = SSL_write(ssl.get(), data, size);
	if (len > 0)
	{
		return ReadBioEncrypted() ? TlsClientError::NoError : TlsClientError::Failed;
	}
	else
	{
		return TlsClientError::Failed;
	}
}

void TlsClient::Shutdown()
{
	initialised = false;
	
	encrypted.clear();
	decrypted.clear();
	
	ssl.reset();
	rbio.reset();
	wbio.reset();
	ctx.reset();
}

TlsClient::SslStatus TlsClient::GetSslStatus(int returnCode)
{
	switch (SSL_get_error(ssl.get(), returnCode))
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

TlsClient::SslStatus TlsClient::Handshake()
{	
	auto ret = SSL_do_handshake(ssl.get());
	
	auto TlsError = GetSslStatus(ret);
	if (TlsError == SslStatus::Pending)
	{
		return ReadBioEncrypted() ? SslStatus::OK : SslStatus::Failed;
	}
	
	return TlsError;
}

bool TlsClient::ReadBioEncrypted()
{
	int bytes = 0;
	do {
		bytes = BIO_read(wbio.get(), encryptCache, sizeof(encryptCache));
		if (bytes > 0)
		{
			QueueEncryptedData(encryptCache, bytes);
		}
		else if (!BIO_should_retry(wbio.get()))
		{
			return false;
		}
	} while (bytes > 0);
	
	return true;
}

void TlsClient::QueueEncryptedData(const uint8_t* data, size_t size)
{
	if (encrypted.full())
	{
		// This is unlikely to happen as we clean up the queue immediately.
		Warning("TLS encrypted queue full. Queue size: %zu.\n", encrypted.size());
	}
	
	encrypted.emplace_back(data, data + size);
}

void TlsClient::QueueDecryptedData(const uint8_t* data, size_t size)
{
	if (decrypted.full())
	{
		// This is unlikely to happen as we clean up the queue immediately.
		Warning("TLS decrypted queue full. Queue size: %zu.\n", decrypted.size());
	}
	
	decrypted.emplace_back(data, data + size);
}
