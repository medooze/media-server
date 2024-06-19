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
	enum class Status
	{ 
		OK,
		Pending,
		Failed
	};
	
	TlsClient();
	
	bool initialize(const char* hostname = nullptr);
	
	Status decrypt(const uint8_t* data, size_t size);

	Status encrypt(const uint8_t* data, size_t size);
	
	template<typename T>
	void popAllDecypted(const T& callback)
	{
		while (!decrypted.empty())
		{	
			callback(std::move(decrypted.front()));
			decrypted.pop_front();
		}
	}

	template<typename T>
	void popAllEncrypted(const T& callback)
	{	
		while (!encrypted.empty())
		{
			callback(std::move(encrypted.front()));
			encrypted.pop_front();
		}
	}
	
	inline bool isInitialised() const
	{
		return initialised;
	}
	
	void shutdown();

private:

	Status getSslStatus(int returnCode);

	Status handshake();
	
	void queueEncryptedData(const uint8_t* data, size_t size);
	
	void queueDecryptedData(const uint8_t* data, size_t size);

	SSL_CTX *ctx = nullptr;
	SSL *ssl = nullptr;

	BIO *rbio = nullptr;
	BIO *wbio = nullptr;
	
	uint8_t decryptCache[MTU];
	uint8_t encryptCache[MTU];
	
	std::deque<std::vector<uint8_t>> encrypted;
	std::deque<std::vector<uint8_t>> decrypted;
	
	bool initialised = false;
};

#endif
