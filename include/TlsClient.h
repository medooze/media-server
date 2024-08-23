#ifndef TLSCLIENT_H
#define TLSCLIENT_H

#include "config.h"

#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <functional>

#include <OpenSSL.h>

class TlsClient 
{
public:
	using Callback = std::function<void(const uint8_t* data, size_t size)>;
	
	enum class SslStatus
	{ 
		OK,
		Pending,
		Failed
	};
	
	enum class TlsClientError
	{
		NoError,
		Pending,
		HandshakeFailed,
		Failed
	};
	
	TlsClient(bool allowAllCertificates = false);
	~TlsClient();
	
	/**
	 * Refer to following link for use of hostname.
	 * https://knowledge.digicert.com/quovadis/ssl-certificates/ssl-general-topics/what-is-sni-server-name-indication
	*/
	bool Initialize(const char* hostname = nullptr);

	SslStatus Handshake();
	
	TlsClientError Decrypt(const uint8_t* data, size_t size, const Callback& callback);

	TlsClientError Encrypt(const uint8_t* data, size_t size);

	bool ReadEncrypted(const Callback& callback);
	
	inline bool IsInitialised() const
	{
		return initialised;
	}
	
	void Shutdown();

private:

	SslStatus GetSslStatus(int returnCode);

	bool ReadBioEncrypted(const Callback& callback);

	std::unique_ptr<SSL_CTX, SSLCtxDeleter> ctx;
	std::unique_ptr<SSL, SSLDeleter> ssl;

	BIO* rbio = nullptr;
	BIO* wbio = nullptr;
	
	uint8_t decryptCache[MTU];
	uint8_t encryptCache[MTU];
	
	bool initialised = false;
	bool isPendingEncrypted = false;
	
	bool allowAllCertificates = false;
};

#endif
