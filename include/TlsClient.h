#ifndef TLSCLIENT_H
#define TLSCLIENT_H

#include "config.h"

#include <stdint.h>
#include <stddef.h>
#include <memory>

#include <OpenSSL.h>

#include "CircularQueue.h"
#include "log.h"

template <typename T, void (*function)(T*)>
struct FunctionDeleter {
  void operator()(T* pointer) const { function(pointer); }
  typedef std::unique_ptr<T, FunctionDeleter> Pointer;
};

template <typename T, void (*function)(T*)>
using DeleteFnPtr = typename FunctionDeleter<T, function>::Pointer;

using BIOPointer = DeleteFnPtr<BIO, BIO_free_all>;
using SSLCtxPointer = DeleteFnPtr<SSL_CTX, SSL_CTX_free>;
using SSLPointer = DeleteFnPtr<SSL, SSL_free>;

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

	SSLCtxPointer ctx;
	SSLPointer ssl;

	BIOPointer rbio;
	BIOPointer wbio;
	
	uint8_t decryptCache[MTU];
	uint8_t encryptCache[MTU];
	
	CircularQueue<std::vector<uint8_t>> encrypted;
	CircularQueue<std::vector<uint8_t>> decrypted;
	
	bool initialised = false;
	bool allowAllCertificates = false;
};

#endif
