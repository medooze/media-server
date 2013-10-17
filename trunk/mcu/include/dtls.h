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
#include "config.h"

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
		F8_128_HMAC_SHA1_80     = 3
	};

	enum Hash
	{
		HASH_SHA1, /*!< SHA-1 fingerprint hash */
	};

	enum Connection {
		CONNECTION_NEW,      /*!< Endpoint wants to use a new connection */
		CONNECTION_EXISTING, /*!< Endpoint wishes to use existing connection */
	};
public:
	class Listener
	{
	public:
		virtual void onDTLSSetup(Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize) = 0;
	};

public:
	DTLSConnection(Listener& listener);
	~DTLSConnection();

	
	int  Init();
	int  SetSuite(Suite suite);
	void SetRemoteSetup(Setup setup);
	void SetRemoteFingerprint(Hash hash, const char *fingerprint);
	int  End();
	void Reset();
	
	int  Read(BYTE* data,int size);
	int  Write(BYTE *buffer,int size);
	int  Renegotiate();
public:
	static bool IsDTLS(BYTE* buffer,int size)	{ return buffer[0]>=20 && buffer[0]<=64; }

protected:
	int SetupSRTP();
	int CheckPending();
public:
	char *certfile;                        /*!< Certificate file */
	char *pvtfile;                         /*!< Private key file */
	char *cipher;                          /*!< Cipher to use */
//	char *cafile;                          /*!< Certificate authority file */
//	char *capath;                          /*!< Path to certificate authority */
	bool dtls_failure;		/*!< Failure occurred during DTLS negotiation */
private:
	class LibraryInit
	{
	public:
		LibraryInit()
		{
			SSL_library_init();
		}
	};
	static const LibraryInit library;
private:
	
	Listener& listener;
	SSL_CTX *ssl_ctx;		/*!< SSL context */
	SSL *ssl;			/*!< SSL session */
	BIO *read_bio;			/*!< Memory buffer for reading */
	BIO *write_bio;			/*!< Memory buffer for writing */
	Setup dtls_setup;		/*!< Current setup state */
	Suite suite;						/*!< SRTP crypto suite */
	char local_fingerprint[160];				/*!< Fingerprint of our certificate */
	unsigned char remote_fingerprint[EVP_MAX_MD_SIZE];	/*!< Fingerprint of the peer certificate */
	Connection connection;		/*!< Whether this is a new or existing connection */
	unsigned int rekey;	/*!< Interval at which to renegotiate and rekey */
	int rekeyid;		/*!< Scheduled item id for rekeying */
};

#endif	/* DTLS_H */

