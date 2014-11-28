/*
 * File:   dtls.h
 * Author: Sergio
 *
 * Created on 19 de septiembre de 2013, 11:18
 */

#ifndef DTLS_H
#define	DTLS_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <string>
#include <map>
#include <vector>
#include "config.h"
#include "log.h"


class DTLSConnection
{
public:
	enum Setup
	{
		SETUP_ACTIVE,   /*!< Endpoint is willing to inititate connections */
		SETUP_PASSIVE,  /*!< Endpoint is willing to accept connections */
		SETUP_ACTPASS,  /*!< Endpoint is willing to both accept and initiate connections */
		SETUP_HOLDCONN, /*!< Endpoint does not want the connection to be established right now */
	};

	enum Suite {
		AES_CM_128_HMAC_SHA1_80 = 1,
		AES_CM_128_HMAC_SHA1_32 = 2,
		F8_128_HMAC_SHA1_80     = 3,
		UNKNOWN_SUITE
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

	enum Connection {
		CONNECTION_NEW,      /*!< Endpoint wants to use a new connection */
		CONNECTION_EXISTING /*!< Endpoint wishes to use existing connection */
	};
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onDTLSSetup(Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize) = 0;
	};

public:
	static void SetCertificate(const char* cert,const char* key);
	static int ClassInit();
	static std::string GetCertificateFingerPrint(Hash hash);
	static bool IsDTLS(BYTE* buffer,int size)		{ return buffer[0]>=20 && buffer[0]<=64; }

private:
	static std::string certfile;	/*!< Certificate file */
	static std::string pvtfile;		/*!< Private key file */
	static std::string cipher;		/*!< Cipher to use */
	static SSL_CTX* ssl_ctx;		/*!< SSL context */
	static Suite suite;				/*!< SRTP crypto suite */
	typedef std::map<Hash, std::string> LocalFingerPrints;
	static LocalFingerPrints localFingerPrints;
	typedef std::vector<Hash> AvailableHashes;
	static AvailableHashes availableHashes;
	static bool hasDTLS;

public:
	DTLSConnection(Listener& listener);
	~DTLSConnection();

	int  Init();
	void SetRemoteSetup(Setup setup);
	void SetRemoteFingerprint(Hash hash, const char *fingerprint);
	void End();
	void Reset();

	int  Read(BYTE* data,int size);
	int  Write(BYTE *buffer,int size);
	int  Renegotiate();

/* Callbacks fired by OpenSSL events. */
public:
	void onSSLInfo(int where, int ret);

protected:
	int  SetupSRTP();
	int  CheckPending();
private:
	Listener& listener;
	SSL *ssl;			/*!< SSL session */
	BIO *read_bio;			/*!< Memory buffer for reading */
	BIO *write_bio;			/*!< Memory buffer for writing */
	Setup dtls_setup;		/*!< Current setup state */
	unsigned char remoteFingerprint[EVP_MAX_MD_SIZE];	/*!< Fingerprint of the peer certificate */
	Hash remoteHash;		/*!< Hash of the peer fingerprint */
	Connection connection;		/*!< Whether this is a new or existing connection */
	unsigned int rekey;	/*!< Interval at which to renegotiate and rekey */
	int rekeyid;		/*!< Scheduled item id for rekeying */
	bool inited;        /*!< Set to true once the SSL stuff is set for this DTLS session */
};

#endif	/* DTLS_H */

