#include <srtp2/srtp.h>
#include "dtls.h"
#include "log.h"

#define SRTP_MASTER_KEY_LENGTH 16
#define SRTP_MASTER_SALT_LENGTH 14
#define SRTP_MASTER_LENGTH (SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH)


/* Initialize static data. */
std::string DTLSConnection::certfile("mcu.crt");
std::string DTLSConnection::pvtfile("mcy.key");
std::string DTLSConnection::cipher("ALL:NULL:eNULL:aNULL");
SSL_CTX* DTLSConnection::ssl_ctx = NULL;
DTLSConnection::Suite DTLSConnection::suite = AES_CM_128_HMAC_SHA1_80;
DTLSConnection::LocalFingerPrints DTLSConnection::localFingerPrints;
DTLSConnection::AvailableHashes DTLSConnection::availableHashes;
bool DTLSConnection::hasDTLS = false;


/* Static callbacks for OpenSSL. */

static inline
int on_ssl_certificate_verify(int preverify_ok, X509_STORE_CTX* ctx)
{
	// Always valid.
	return 1;
}

static inline
void on_ssl_info(const SSL* ssl, int where, int ret)
{
	DTLSConnection *conn = (DTLSConnection*)SSL_get_ex_data(ssl, 0);
	conn->onSSLInfo(where, ret);
}


/* Static methods. */

void DTLSConnection::SetCertificate(const char* cert,const char* key)
{
	//Set certificate files
	DTLSConnection::certfile.assign(cert);
	DTLSConnection::pvtfile.assign(key);
}

int DTLSConnection::ClassInit()
{
	Log("-DTLSConnection::ClassInit()\n");

	EC_KEY* ecdh = NULL;

	/* Create a single SSL context. */

	DTLSConnection::ssl_ctx = SSL_CTX_new(DTLSv1_method());
	if (! ssl_ctx) {
		// Print SSL error.
		ERR_print_errors_fp(stderr);
		return Error("-DTLSConnection::ClassInit() | No SSL context\n");
	}

	// Set certificate.
	if (! SSL_CTX_use_certificate_file(ssl_ctx, certfile.c_str(), SSL_FILETYPE_PEM))
		return Error("-DTLSConnection::ClassInit() | Specified certificate file '%s' could not be used\n", certfile.c_str());

	if (! SSL_CTX_use_PrivateKey_file(ssl_ctx, pvtfile.c_str(), SSL_FILETYPE_PEM) || !SSL_CTX_check_private_key(ssl_ctx))
		return Error("-DTLSConnection::ClassInit() | Specified private key file '%s' could not be used\n",pvtfile.c_str());

	if (! SSL_CTX_set_cipher_list(ssl_ctx, cipher.c_str()))
		return Error("-DTLSConnection::ClassInit() | Invalid cipher specified in cipher list '%s' for DTLS-SRTP\n",cipher.c_str());

	//Disable automatic MTU discovery
	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_QUERY_MTU);

	// Enable ECDH ciphers.
	// DOC: http://en.wikibooks.org/wiki/OpenSSL/Diffie-Hellman_parameters
	// NOTE: https://code.google.com/p/chromium/issues/detail?id=406458
	// For OpenSSL >= 1.0.2:
	#if (OPENSSL_VERSION_NUMBER >= 0x10002000L)
		SSL_CTX_set_ecdh_auto(ssl_ctx, 1);
	#else
		ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
		if (! ecdh) {
			return Error("-DTLSConnection::ClassInit() | EC_KEY_new_by_curve_name() failed\n");
		}
		if (SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh) != 1) {
			return Error("-DTLSConnection::ClassInit() | SSL_CTX_set_tmp_ecdh() failed\n");
		}
		EC_KEY_free(ecdh);
		ecdh = NULL;
	#endif

	// Don't use session cache.
	SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);

	// Set look ahead
	// See -> https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=775502
	SSL_CTX_set_read_ahead(ssl_ctx,true);

	// Require cert from client (mandatory for WebRTC).
	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, on_ssl_certificate_verify);

	// Set SSL info callback.
	SSL_CTX_set_info_callback(ssl_ctx, on_ssl_info);

	// Set suite.
	switch(suite) {
		case AES_CM_128_HMAC_SHA1_80:
			SSL_CTX_set_tlsext_use_srtp(ssl_ctx, "SRTP_AES128_CM_SHA1_80");
			break;
		case AES_CM_128_HMAC_SHA1_32:
			SSL_CTX_set_tlsext_use_srtp(ssl_ctx, "SRTP_AES128_CM_SHA1_32");
			break;
		default:
			return Error("-DTLSConnection::ClassInit() | Unsupported suite [%d] specified for DTLS-SRTP\n",suite);
	}


	/* Map for local certificate fingerprints. */

	BIO* certbio;
	X509* cert;

	// Fill the DTLSConnection::availableHashes vector.
	DTLSConnection::availableHashes.push_back(SHA1);
	DTLSConnection::availableHashes.push_back(SHA224);
	DTLSConnection::availableHashes.push_back(SHA256);
	DTLSConnection::availableHashes.push_back(SHA384);
	DTLSConnection::availableHashes.push_back(SHA512);

	// Create BIO for certificate file.
	certbio = BIO_new(BIO_s_file());

	// Read certificate filename.
	if (! BIO_read_filename(certbio, certfile.c_str())) {
		return Error("-DTLSConnection::ClassInit() | Could not read certificate filename [%s]\n",certfile.c_str());
	}

	// Read certificate in X509 format.
	cert = PEM_read_bio_X509(certbio, NULL, 0, NULL);
	if (! cert) {
		return Error("-DTLSConnection::ClassInit() | Could not read X509 certificate from filename [%s]\n",certfile.c_str());
	}

	// Iterate the DTLSConnection::availableHashes.
	for(int i = 0; i < availableHashes.size(); i++) {
		Hash hash = availableHashes[i];
		unsigned int size;
		unsigned char fingerprint[EVP_MAX_MD_SIZE];
		char hex_fingerprint[EVP_MAX_MD_SIZE*3+1] = {0};

		switch (hash) {
			case SHA1:
				X509_digest(cert, EVP_sha1(), fingerprint, &size);
				break;
			case SHA224:
				X509_digest(cert, EVP_sha224(), fingerprint, &size);
				break;
			case SHA256:
				X509_digest(cert, EVP_sha256(), fingerprint, &size);
				break;
			case SHA384:
				X509_digest(cert, EVP_sha384(), fingerprint, &size);
				break;
			case SHA512:
				X509_digest(cert, EVP_sha512(), fingerprint, &size);
				break;
		}

		// Check size.
		if (! size) {
			return Error("-DTLSConnection::ClassInit() | Wrong X509 certificate size\n");
		}

		// Convert to hex format.
		for (int j = 0; j < size; j++)
			sprintf(hex_fingerprint+j*3, "%.2X:", fingerprint[j]);

		// End string.
		hex_fingerprint[size*3-1] = 0;

		// Store in the map.
		DTLSConnection::localFingerPrints[hash] = std::string(hex_fingerprint);
	}

	// Free BIO.
	BIO_free_all(certbio);

	// OK, we have DTLS.
	DTLSConnection::hasDTLS = true;

	//OK
	return 1;
}

std::string DTLSConnection::GetCertificateFingerPrint(Hash hash)
{
	return DTLSConnection::localFingerPrints[hash];
}


/* Instance methods. */

DTLSConnection::DTLSConnection(Listener& listener) : listener(listener)
{
	//Set default values
	rekey		  	     = 0;
	dtls_setup		     = SETUP_PASSIVE;
	connection		     = CONNECTION_NEW;
	ssl			     = NULL;		/*!< SSL session */
	read_bio		     = NULL;		/*!< Memory buffer for reading */
	write_bio		     = NULL;		/*!< Memory buffer for writing */
	inited			     = false;
	remoteHash		     = UNKNOWN_HASH;
	//Reset remote fingerprint
	memset(remoteFingerprint,0,EVP_MAX_MD_SIZE);
}

DTLSConnection::~DTLSConnection()
{
	End();
}

int DTLSConnection::Init()
{
	Log(">DTLSConnection::Init()\n");

	if (! DTLSConnection::hasDTLS)
		return Error("-DTLSConnection::Init() | no DTLS\n");

	if (!(ssl = SSL_new(ssl_ctx)))
		return Error("-DTLSConnection::Init() | Failed to allocate memory for SSL context on \n");

	SSL_set_ex_data(ssl, 0, this);

	if (!(read_bio = BIO_new(BIO_s_mem())))
	{
		SSL_free(ssl);
		return Error("-DTLSConnection::Init() | Failed to allocate memory for inbound SSL traffic on \n");
	}
	BIO_set_mem_eof_return(read_bio, -1);

	if (!(write_bio = BIO_new(BIO_s_mem())))
	{
		BIO_free(read_bio);
		SSL_free(ssl);
		return Error("-DTLSConnection::Init() | Failed to allocate memory for outbound SSL traffic on \n");
	}
	BIO_set_mem_eof_return(write_bio, -1);

	//Set MTU of datagrams so it fits in an UDP packet
	SSL_set_mtu(ssl, RTPPAYLOADSIZE);
	//DTLS_set_link_mtu(ssl, MTU);
	BIO_ctrl(write_bio, BIO_CTRL_DGRAM_SET_MTU, RTPPAYLOADSIZE, NULL);

	SSL_set_bio(ssl, read_bio, write_bio);

	switch(dtls_setup)
	{
		case SETUP_ACTIVE:
			Debug("-DTLSConnection::Init() | we are SETUP_ACTIVE\n");
			SSL_set_connect_state(ssl);
			break;
		case SETUP_PASSIVE:
			Debug("-DTLSConnection::Init() | we are SETUP_PASSIVE\n");
			SSL_set_accept_state(ssl);
			break;
	}

	//New connection
	connection = CONNECTION_NEW;

	//Start handshake
	SSL_do_handshake(ssl);

	//Now we are ready to read and write DTLS packets.
	inited = true;

	Log("<DTLSConnection::Init()\n");

	return 1;
}

void DTLSConnection::End()
{
	Log("-DTLSConnection::End()\n");

	// NOTE: Don't use BIO_free() for write_bio and read_bio as they are
	// automatically freed by SSL_free().

	if (ssl) {
		SSL_free(ssl);
		ssl = NULL;
	}
}

void DTLSConnection::Reset()
{
	Log("-DTLSConnection::Reset()\n");

	/* If the SSL session is not yet finalized don't bother resetting */
	if (!SSL_is_init_finished(ssl))
		return;

	SSL_shutdown(ssl);

	connection = CONNECTION_NEW;
}

void DTLSConnection::SetRemoteSetup(Setup remote)
{
	Log("-DTLSConnection::SetRemoteSetup() | [remote:%d]\n", remote);

	if (! DTLSConnection::hasDTLS) {
		Error("no DTLS\n");
		return;
	}

	Setup old = dtls_setup;

	switch (remote)
	{
		case SETUP_ACTIVE:
			dtls_setup = SETUP_PASSIVE;
			break;
		case SETUP_PASSIVE:
			dtls_setup = SETUP_ACTIVE;
			break;
		case SETUP_ACTPASS:
			/* We can't respond to an actpass setup with actpass ourselves... so respond with active, as we can initiate connections */
			if (dtls_setup == SETUP_ACTPASS)
				dtls_setup = SETUP_ACTIVE;
			break;
		case SETUP_HOLDCONN:
			dtls_setup = SETUP_HOLDCONN;
			break;
		default:
			/* This should never occur... if it does exit early as we don't know what state things are in */
			return;
	}

	/* If the setup state did not change we go on as if nothing happened */
	if (old == dtls_setup || !ssl)
		return;

	switch (dtls_setup)
	{
		case SETUP_ACTIVE:
			Debug("-DTLSConnection::SetRemoteSetup() | we are SETUP_ACTIVE\n");
			SSL_set_connect_state(ssl);
			break;
		case SETUP_PASSIVE:
			Debug("-DTLSConnection::SetRemoteSetup() | we are SETUP_PASSIVE\n");
			SSL_set_accept_state(ssl);
			break;
		case SETUP_HOLDCONN:
		default:
			return;
	}

	return;
}

void DTLSConnection::SetRemoteFingerprint(Hash hash, const char *fingerprint)
{
	if (! DTLSConnection::hasDTLS) {
		Error("-DTLSConnection::SetRemoteFingerprint() | no DTLS\n");
		return;
	}

	remoteHash = hash;
	char* tmp = strdup(fingerprint);
	char* value;
	int pos = 0;

	while ((value = strsep(&tmp, ":")) && (pos != (EVP_MAX_MD_SIZE - 1)))
		sscanf(value, "%02x", (unsigned int*)&remoteFingerprint[pos++]);

	free(tmp);
}

int DTLSConnection::Read(BYTE* data,int size)
{
	if (! DTLSConnection::hasDTLS) 
		return Error("-DTLSConnection::Read() | no DTLS\n");

	if (! inited)
		return Error("-DTLSConnection::Read() | SSL not yet ready\n");

	if (BIO_ctrl_pending(write_bio))
		return BIO_read(write_bio, data, size);

	return 0;
}

inline
void DTLSConnection::onSSLInfo(int where, int ret)
{
	Debug("[where:%d, ret:%d] | SSL status: %s | handshake done: %s\n", where, ret, SSL_state_string_long(this->ssl), SSL_is_init_finished(this->ssl) ? "yes" : "no");

	if (where & SSL_CB_HANDSHAKE_START)
	{
		Debug("-DTLSConnection::onSSLInfo() | DTLS handshake starts\n");
	}
	else if (where & SSL_CB_HANDSHAKE_DONE)
	{
		Log("-DTLSConnection::onSSLInfo() | DTLS handshake done\n");

		/* Any further connections will be existing since this is now established */
		connection = CONNECTION_EXISTING;

		/* Use the keying material to set up key/salt information */
		SetupSRTP();
	}

	// NOTE: checking SSL_get_shutdown(this->ssl) & SSL_RECEIVED_SHUTDOWN here upon
	// receipt of a close alert does not work.
}

int DTLSConnection::Renegotiate()
{
	SSL_renegotiate(ssl);
	SSL_do_handshake(ssl);

	rekeyid = -1;
	return 1;
}

int DTLSConnection::SetupSRTP()
{
	if (! DTLSConnection::hasDTLS)
		return Error("-DTLSConnection::SetupSRTP() | no DTLS\n");

	X509* certificate;
	unsigned char fingerprint[EVP_MAX_MD_SIZE];
	unsigned int size=0;
	const EVP_MD* hash_function;
	std::string hash_str;

	BYTE material[SRTP_MASTER_LENGTH * 2];
	BYTE localMasterKey[SRTP_MASTER_LENGTH];
	BYTE remoteMasterKey[SRTP_MASTER_LENGTH];
	BYTE *local_key, *local_salt, *remote_key, *remote_salt;

	if (!(certificate = SSL_get_peer_certificate(ssl)))
		return Error("-DTLSConnection::SetupSRTP() | no certificate was provided by the peer\n");

	switch (remoteHash)
	{
		case SHA1:
			hash_function = EVP_sha1();
			hash_str = "SHA-1";
			break;
		case SHA224:
			hash_function = EVP_sha224();
			hash_str = "SHA-224";
			break;
		case SHA256:
			hash_function = EVP_sha256();
			hash_str = "SHA-256";
			break;
		case SHA384:
			hash_function = EVP_sha384();
			hash_str = "SHA-384";
			break;
		case SHA512:
			hash_function = EVP_sha512();
			hash_str = "SHA-512";
			break;
		default:
			X509_free(certificate);
			return Error("-DTLSConnection::SetupSRTP() | unknown remote hash, cannot verify remote fingerprint\n");
	}

	if (!X509_digest(certificate, hash_function, fingerprint, &size) || !size || memcmp(fingerprint, remoteFingerprint, size))
	{
		X509_free(certificate);
		return Error("-DTLSConnection::SetupSRTP() | fingerprint in remote SDP does not match that of peer certificate (hash %s)\n", hash_str.c_str());
	}

	Debug("-DTLSConnection::SetupSRTP() | fingerprint in remote SDP matches that of peer certificate (hash %s)\n", hash_str.c_str());
	X509_free(certificate);

	/* Produce key information and set up SRTP */

	if (! SSL_export_keying_material(ssl, material, SRTP_MASTER_LENGTH * 2, "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0))
		return Error("-DTLSConnection::SetupSRTP() | Unable to extract SRTP keying material from DTLS-SRTP negotiation on RTP instance \n");

	/* Whether we are acting as a server or client determines where the keys/salts are */

	if (dtls_setup == SETUP_ACTIVE)
	{
		local_key = material;
		remote_key = local_key + SRTP_MASTER_KEY_LENGTH;
		local_salt = remote_key + SRTP_MASTER_KEY_LENGTH;
		remote_salt = local_salt + SRTP_MASTER_SALT_LENGTH;
	} else	{
		remote_key = material;
		local_key = remote_key + SRTP_MASTER_KEY_LENGTH;
		remote_salt = local_key + SRTP_MASTER_KEY_LENGTH;
		local_salt = remote_salt + SRTP_MASTER_SALT_LENGTH;
	}

	//Create local master key
	memcpy(localMasterKey,local_key,SRTP_MASTER_KEY_LENGTH);
	memcpy(localMasterKey+SRTP_MASTER_KEY_LENGTH,local_salt,SRTP_MASTER_SALT_LENGTH);
	//Create remote master key
	memcpy(remoteMasterKey,remote_key,SRTP_MASTER_KEY_LENGTH);
	memcpy(remoteMasterKey+SRTP_MASTER_KEY_LENGTH,remote_salt,SRTP_MASTER_SALT_LENGTH);

	//Fire event
	listener.onDTLSSetup(suite,localMasterKey,SRTP_MASTER_LENGTH,remoteMasterKey,SRTP_MASTER_LENGTH);

	return 1;
}

int DTLSConnection::Write(BYTE *buffer,int size)
{
	if (! DTLSConnection::hasDTLS)
		return Error("-DTLSConnection::Write() | no DTLS\n");

	if (! inited) {
		return Error("-DTLSConnection::Write() | SSL not yet ready\n");
	}

	BIO_write(read_bio, buffer, size);

	if (SSL_read(ssl, buffer, size)<0)
		return Error("-DTLSConnection::Write() | SSL_read error\n");

	// Check if the peer sent close alert or a fatal error happened.
	if (SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) {
		Debug("-DTLSConnection::Write() | SSL_RECEIVED_SHUTDOWN on instance '%p', resetting SSL\n", this);

		int err = SSL_clear(ssl);
		if (err == 0)
			Error("-DTLSConnection::Write() | SSL_clear() failed: %s", ERR_error_string(ERR_get_error(), NULL));

		return 0;
	}

	return 1;
}

int DTLSConnection::CheckPending()
{
	if (!write_bio)
		return 0;

	return BIO_ctrl_pending(write_bio);
}
