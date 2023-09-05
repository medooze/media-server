#ifndef DTLS_H
#define	DTLS_H

#include <openssl/opensslconf.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <atomic>
#include <string>
#include <map>
#include <vector>
#include "config.h"
#include "log.h"
#include "Datachannels.h"

class DTLSConnection
{
public:
	enum Setup
	{
		SETUP_ACTIVE,   // Endpoint is willing to inititate connections 
		SETUP_PASSIVE,  // Endpoint is willing to accept connections 
		SETUP_ACTPASS,  // Endpoint is willing to both accept and initiate connections 
		SETUP_HOLDCONN, // Endpoint does not want the connection to be established right now 
	};

	enum Suite {
		AES_CM_128_HMAC_SHA1_80 = 1,
		AES_CM_128_HMAC_SHA1_32 = 2,
		AEAD_AES_128_GCM	= 7,
		AEAD_AES_256_GCM	= 8,
		UNKNOWN_SUITE		= 0
	};

	enum Hash
	{
		SHA1,
		SHA224,
		SHA256,
		SHA384,
		SHA512,
		UNKNOWN_HASH
	};

public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onDTLSPendingData() = 0;
		virtual void onDTLSSetup(Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize) = 0;
		virtual void onDTLSSetupError() = 0;
		virtual void onDTLSShutdown() = 0;
	};

public:
	static void SetCertificate(const char* cert,const char* key);
	static int Initialize();
	static int Terminate();
	static std::string GetCertificateFingerPrint(Hash hash);
	static bool IsDTLS(const BYTE* buffer,const DWORD size)		{ return buffer[0]>=20 && buffer[0]<=64; }
	static Suite SuiteFromName(const char* suite) 
	{
		if (strcmp(suite,"SRTP_AES128_CM_SHA1_80")==0)
			return AES_CM_128_HMAC_SHA1_80;
		else if (strcmp(suite,"SRTP_AES128_CM_SHA1_32")==0) 
			return AES_CM_128_HMAC_SHA1_32;
		else if (strcmp(suite,"SRTP_AEAD_AES_128_GCM")==0)
			return AEAD_AES_128_GCM;
		else if (strcmp(suite,"SRTP_AEAD_AES_256_GCM")==0)
			return AEAD_AES_256_GCM;
		return UNKNOWN_SUITE;
	}

	static std::pair<size_t,size_t> SuiteKeySaltLength(const Suite suite)
	{
		switch (suite)
		{
			case AES_CM_128_HMAC_SHA1_80:
			case AES_CM_128_HMAC_SHA1_32:
				// SRTP_AES128_CM_HMAC_SHA1_32 and SRTP_AES128_CM_HMAC_SHA1_80 are defined
				// in RFC 5764 to use a 128 bits key and 112 bits salt for the cipher.
				return std::make_pair(16,14);
			case AEAD_AES_128_GCM:
				// SRTP_AEAD_AES_128_GCM is defined in RFC 7714 to use a 128 bits key and
				// a 96 bits salt for the cipher.
				return std::make_pair(16,12);
			case AEAD_AES_256_GCM:
				// SRTP_AEAD_AES_256_GCM is defined in RFC 7714 to use a 256 bits key and
				// a 96 bits salt for the cipher.
				return std::make_pair(32,12);
			default:
				return std::make_pair(0,0);
		}
	}

private:
	static int GenerateCertificate();
	static int ReadCertificate();
	
private:
	typedef std::map<Hash, std::string> LocalFingerPrints;
	typedef std::vector<Hash> AvailableHashes;
private:
	static std::string	certfile;		// Certificate file name
	static std::string	pvtfile;		// Private key file name
	static std::string	cipher;			// Cipher to use 
	static SSL_CTX*		ssl_ctx;		// SSL context 
	static X509*		certificate;		// SSL context 
	static EVP_PKEY*	privateKey;		// SSL context 
	static LocalFingerPrints localFingerPrints;
	static AvailableHashes	availableHashes;
	static bool		hasDTLS;

public:
	DTLSConnection(Listener& listener,TimeService& timeService,datachannels::Transport& sctp);
	~DTLSConnection();

	void SetSRTPProtectionProfiles(const std::string& profiles);
	int  Init();
	void SetRemoteSetup(Setup setup);
	void SetRemoteFingerprint(Hash hash, const char *fingerprint);
	void End();

	Setup GetSetup() const { return setup; }
	
	int  Read(BYTE* data,DWORD size);
	int  Write(const BYTE *buffer,DWORD size);
	int  HandleTimeout();
	int  Renegotiate();
	void  Reset();

// Callbacks fired by OpenSSL events. 
public:
	void onSSLInfo(int where, int ret);

protected:
	int  SetupSRTP();
	void Shutdown();
	void CheckPending();
private:
	Listener& listener;
	TimeService& timeService;
	Timer::shared timeout;			// DTLS timout handler
	datachannels::Transport &sctp;		// SCTP transport
	SSL *ssl	= nullptr;		// SSL session 
	BIO *read_bio	= nullptr;		// Memory buffer for reading 
	BIO *write_bio	= nullptr;		// Memory buffer for writing 
	Setup setup	= SETUP_PASSIVE;	// Current setup state 
	Hash remoteHash = UNKNOWN_HASH;		// Hash of the peer fingerprint 
	unsigned char remoteFingerprint[EVP_MAX_MD_SIZE] = {};	// Fingerprint of the peer certificate 
	std::atomic<bool> inited;	// Set to true once the SSL stuff is set for this DTLS session 
	std::string profiles;		// Overrriden list of srtp profiles
};

#endif

