#ifndef TLSCLIENT_H
#define TLSCLIENT_H

#include "config.h"

#include <stdint.h>
#include <stddef.h>
#include <deque>

#include <OpenSSL.h>

#include "log.h"

class TlsClient 
{
public:
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
	
	TlsClientError Decrypt(const uint8_t* data, size_t size);

	TlsClientError Encrypt(const uint8_t* data, size_t size);
	
	template<typename T>
	void PopAllDecypted(const T& callback)
	{
		while (!decrypted.empty())
		{	
			callback(std::move(decrypted.front()));
			decrypted.pop_front();
		}
	}

	template<typename T>
	void PopAllEncrypted(const T& callback)
	{	
		while (!encrypted.empty())
		{
			callback(std::move(encrypted.front()));
			encrypted.pop_front();
		}
	}
	
	inline bool IsInitialised() const
	{
		return initialised;
	}
	
	void Shutdown();

private:

	SslStatus GetSslStatus(int returnCode);

	bool ReadBioEncrypted();
	
	void QueueEncryptedData(const uint8_t* data, size_t size);
	
	void QueueDecryptedData(const uint8_t* data, size_t size);

	SSL_CTX *ctx = nullptr;
	SSL *ssl = nullptr;

	BIO *rbio = nullptr;
	BIO *wbio = nullptr;
	
	uint8_t decryptCache[MTU];
	uint8_t encryptCache[MTU];
	
	std::deque<std::vector<uint8_t>> encrypted;
	std::deque<std::vector<uint8_t>> decrypted;
	
	bool initialised = false;
	bool allowAllCertificates = false;
};

#endif
