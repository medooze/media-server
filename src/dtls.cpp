#include "tracing.h"
#include <srtp2/srtp.h>
#include "dtls.h"
#include "log.h"
#include "use.h"
#include "Datachannels.h"

using namespace std::chrono_literals;
	
// Initialize static data
std::string		DTLSConnection::certfile("");
std::string		DTLSConnection::pvtfile("");
std::string		DTLSConnection::cipher("ALL:NULL:eNULL:aNULL");
SSL_CTX*		DTLSConnection::ssl_ctx		= NULL;
X509*			DTLSConnection::certificate	= NULL;
EVP_PKEY*		DTLSConnection::privateKey	= NULL;
bool			DTLSConnection::hasDTLS		= false;

DTLSConnection::LocalFingerPrints	DTLSConnection::localFingerPrints;
DTLSConnection::AvailableHashes		DTLSConnection::availableHashes;

// Static methods. 
void DTLSConnection::SetCertificate(const char* cert,const char* key)
{
	//Log
	Debug("-DTLSConnection::SetCertificate() | Set SSL certificate files [crt:\"%s\",key:\"%s\"]\n",cert,key);
	//Set certificate files
	DTLSConnection::certfile.assign(cert);
	DTLSConnection::pvtfile.assign(key);
}

int DTLSConnection::GenerateCertificate()
{
	TRACE_EVENT("dtls", "DTLSConnection::GenerateCertificate");
	Debug(">DTLSConnection::GenerateCertificate()\n");
	
	int ret = 0;
	BIGNUM* bne = NULL;
	RSA* rsa_key = NULL;
	int num_bits = 2048;
	X509_NAME* cert_name = NULL;

	// Create a big number object.
	bne = BN_new();
	if (!bne)
	{
		Error("BN_new() failed");
		goto error;
	}

	ret = BN_set_word(bne, RSA_F4); // RSA_F4 == 65537.
	if (ret == 0)
	{
		Error("BN_set_word() failed");
		goto error;
	}

	// Generate a RSA key.
	rsa_key = RSA_new();
	if (!rsa_key)
	{
		Error("RSA_new() failed");
		goto error;
	}

	// This takes some time.
	ret = RSA_generate_key_ex(rsa_key, num_bits, bne, NULL);
	if (ret == 0)
	{
		Error("RSA_generate_key_ex() failed");
		goto error;
	}

	// Create a private key object (needed to hold the RSA key).
	privateKey = EVP_PKEY_new();
	if (!privateKey)
	{
		Error("EVP_PKEY_new() failed");
		goto error;
	}

	ret = EVP_PKEY_assign_RSA(privateKey, rsa_key);
	if (ret == 0)
	{
		Error("EVP_PKEY_assign_RSA() failed");
		goto error;
	}
	// The RSA key now belongs to the private key, so don't clean it up separately.
	rsa_key = NULL;

	// Create the X509 certificate.
	certificate = X509_new();
	if (!certificate)
	{
		Error("X509_new() failed");
		goto error;
	}

	// Set version 3 (note that 0 means version 1).
	X509_set_version(certificate, 2);

	// Set serial number (avoid default 0).
	ASN1_INTEGER_set(X509_get_serialNumber(certificate), (long)getTimeMS());

	// Set valid period.
	X509_gmtime_adj(X509_get_notBefore(certificate)	, -1*60*60*24*365*10); // - 10 years.
	X509_gmtime_adj(X509_get_notAfter(certificate)	, 60*60*24*365*10); // 10 years.

	// Set the public key for the certificate using the key.
	ret = X509_set_pubkey(certificate, privateKey);
	if (ret == 0)
	{
		Error("X509_set_pubkey() failed");
		goto error;
	}

	// Set certificate fields.
	cert_name = X509_get_subject_name(certificate);
	if (!cert_name)
	{
		Error("X509_get_subject_name() failed");
		goto error;
	}
	
	X509_NAME_add_entry_by_txt(cert_name, "O", MBSTRING_ASC, (BYTE*)"medooze", -1, -1, 0);
	X509_NAME_add_entry_by_txt(cert_name, "CN", MBSTRING_ASC, (BYTE*)"medooze", -1, -1, 0);

	// It is self-signed so set the issuer name to be the same as the subject.
	ret = X509_set_issuer_name(certificate, cert_name);
	if (ret == 0)
	{
		Error("X509_set_issuer_name() failed");
		goto error;
	}

	// Sign the certificate with its own private key.
	ret = X509_sign(certificate, privateKey, EVP_sha1());
	if (ret == 0)
	{
		Error("X509_sign() failed");
		goto error;
	}

	// Free stuff and return.
	BN_free(bne);
	
	Debug("<DTLSConnection::GenerateCertificate()\n");
	
	return 1;

error:
	if (bne)
		BN_free(bne);
	if (rsa_key && !privateKey)
		RSA_free(rsa_key);
	if (privateKey)
	{
		EVP_PKEY_free(privateKey); // NOTE: This also frees the RSA key.
		privateKey = NULL;
	}
	if (certificate)
	{
		X509_free(certificate);
		certificate = NULL;
	}

	//Error
	return Error("DTLS certificate and private key generation failed");
	
}

int DTLSConnection::ReadCertificate()
{
	TRACE_EVENT("dtls", "DTLSConnection::ReadCertificate");
	
	//Open certificate file
	FILE* file = fopen(certfile.c_str(), "r");
	
	//Check
	if (!file)
		return Error("error reading DTLS certificate file: %d\n", errno);
	
	//REad certificate
	certificate = PEM_read_X509(file, NULL, NULL, NULL);
	
	//Close file
	fclose(file);
	
	//Check certificate
	if (!certificate)
		return Error("PEM_read_X509() failed\n");
	
	//Open private kwy
	file = fopen(pvtfile.c_str(), "r");
	
	//Check
	if (!file)
		return Error("error reading DTLS private key file: %d\n", errno);
	
	//Load key
	privateKey = PEM_read_PrivateKey(file, NULL, NULL, NULL);
	
	//Clse file
	fclose(file);
	
	//Check it is correct
	if (!privateKey)
		return Error("PEM_read_PrivateKey() failed");

	//Done
	return 1;
}

int DTLSConnection::Initialize()
{
	TRACE_EVENT("dtls", "Initialize");
	Debug(">DTLSConnection::Initialize()\n");

	// Create a single SSL context
	ssl_ctx = SSL_CTX_new(DTLS_method());
	
	//Check result
	if (!ssl_ctx) 
	{
		// Print SSL error.
		ERR_print_errors_fp(stderr);
		return Error("-DTLSConnection::Initialize() | No SSL context\n");
	}

	//Disable automatic MTU discovery
	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_QUERY_MTU);

	// Enable ECDH ciphers.
	SSL_CTX_set_ecdh_auto(ssl_ctx, 1);

	// Don't use session cache.
	SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);

	// Set look ahead
	// See -> https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=775502
	SSL_CTX_set_read_ahead(ssl_ctx,true);

	// Require cert from client (mandatory for WebRTC).
	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, [](int preverify_ok, X509_STORE_CTX* ctx) -> int {
		// Always valid.
		return 1;
	});

	// Set SSL info callback.
	SSL_CTX_set_info_callback(ssl_ctx, [](const SSL* ssl, int where, int ret) {
		DTLSConnection *conn = (DTLSConnection*)SSL_get_ex_data(ssl, 0);
		conn->onSSLInfo(where, ret);
	});

	// Try to use GCM suite
	if (SSL_CTX_set_tlsext_use_srtp(ssl_ctx, "SRTP_AES128_CM_SHA1_80;SRTP_AEAD_AES_128_GCM;SRTP_AEAD_AES_256_GCM")!=0)
		//Set it without GCM
		SSL_CTX_set_tlsext_use_srtp(ssl_ctx, "SRTP_AES128_CM_SHA1_80");
	
	//If we have a pre-generated certificate and private key
	if (certfile.size()>0 && pvtfile.size()>0)
	{
		//Read it
		if (!ReadCertificate())
			return Error("Could not read SSL files\n");
	} else {
		//Generate them it
		if (!GenerateCertificate())
			return Error("Could not generate SSL certificate or private key files\n");
	}
	
	// Set certificate.
	if (! SSL_CTX_use_certificate(ssl_ctx, certificate))
		return Error("-DTLSConnection::Initialize() | Specified certificate file '%s' could not be used\n", certfile.c_str());

	//Set private key
	if (! SSL_CTX_use_PrivateKey(ssl_ctx, privateKey) || !SSL_CTX_check_private_key(ssl_ctx))
		return Error("-DTLSConnection::Initialize() | Specified private key file '%s' could not be used\n",pvtfile.c_str());

	//Set cipher list
	if (! SSL_CTX_set_cipher_list(ssl_ctx, cipher.c_str()))
		return Error("-DTLSConnection::Initialize() | Invalid cipher specified in cipher list '%s' for DTLS-SRTP\n",cipher.c_str());
	
	// Fill the DTLSConnection::availableHashes vector.
	DTLSConnection::availableHashes.push_back(SHA1);
	DTLSConnection::availableHashes.push_back(SHA224);
	DTLSConnection::availableHashes.push_back(SHA256);
	DTLSConnection::availableHashes.push_back(SHA384);
	DTLSConnection::availableHashes.push_back(SHA512);

	// Iterate the DTLSConnection::availableHashes.
	for(DWORD i = 0; i < availableHashes.size(); i++) 
	{
		Hash hash = availableHashes[i];
		unsigned int size;
		unsigned char fingerprint[EVP_MAX_MD_SIZE];
		char hex_fingerprint[EVP_MAX_MD_SIZE*3+1] = {0};

		switch (hash)
		{
			case SHA1:
				X509_digest(certificate, EVP_sha1(), fingerprint, &size);
				break;
			case SHA224:
				X509_digest(certificate, EVP_sha224(), fingerprint, &size);
				break;
			case SHA256:
				X509_digest(certificate, EVP_sha256(), fingerprint, &size);
				break;
			case SHA384:
				X509_digest(certificate, EVP_sha384(), fingerprint, &size);
				break;
			case SHA512:
				X509_digest(certificate, EVP_sha512(), fingerprint, &size);
				break;
			default:
				return Error("-DTLSConnection::Initialize() | Unknown hash [%d]\n",hash);
		}

		// Check size.
		if (! size) 
			return Error("-DTLSConnection::Initialize() | Wrong X509 certificate size\n");

		// Convert to hex format.
		for (DWORD j = 0; j < size; j++)
			sprintf(hex_fingerprint+j*3, "%.2X:", fingerprint[j]);

		// End string.
		hex_fingerprint[size*3-1] = 0;

		// Store in the map.
		DTLSConnection::localFingerPrints[hash] = std::string(hex_fingerprint);
		
		Debug("-LocalFingerprint %d %s\n",hash, DTLSConnection::localFingerPrints[hash].c_str());
	}

	// OK, we have DTLS.
	DTLSConnection::hasDTLS = true;

	Debug("<DTLSConnection::Initialize(%lu)\n",availableHashes.size());
	//OK
	return 1;
}

int DTLSConnection::Terminate()
{
	TRACE_EVENT("dtls", "DTLSConnection::Terminate");
	Debug("-DTLSConnection::Terminate()\n");
	
	//Free stuff
	if (privateKey)
		EVP_PKEY_free(privateKey);
	if (certificate)
		X509_free(certificate);
	if (ssl_ctx)
		SSL_CTX_free(ssl_ctx);
	
	//Reset values
	privateKey = nullptr;
	certificate = nullptr;
	ssl_ctx = nullptr;
	
	//All done
	return 1;
}

std::string DTLSConnection::GetCertificateFingerPrint(Hash hash)
{
	return DTLSConnection::localFingerPrints[hash];
}


DTLSConnection::DTLSConnection(Listener& listener,TimeService& timeService,datachannels::Transport& sctp) :
	listener(listener),
	timeService(timeService),
	sctp(sctp),
	inited(false)
{
}

DTLSConnection::~DTLSConnection()
{
	End();
}

void DTLSConnection::SetSRTPProtectionProfiles(const std::string& profiles)
{
	Log("-DTLSConnection::SetSRTPProtectionProfiles() [profiles:'%s']\n",profiles.c_str());
	//Store them
	this->profiles = profiles;
}

int DTLSConnection::Init()
{
	TRACE_EVENT("dtls", "DTLSConnection::Init");
	Log(">DTLSConnection::Init()\n");

	if (! DTLSConnection::hasDTLS)
		return Error("-DTLSConnection::Init() | no DTLS\n");

	if (!(ssl = SSL_new(ssl_ctx)))
		return Error("-DTLSConnection::Init() | Failed to allocate memory for SSL context on \n");
	
	//Overrride default profiles if eny
	if (!profiles.empty())
	{
		//Set them
		if (SSL_set_tlsext_use_srtp(ssl, profiles.c_str()))
		{
			SSL_free(ssl);
			ssl = nullptr;
			return Error("-DTLSConnection::Init() | Failed to override SRTP profiles [profiles:'%s']\n",profiles.c_str());
		}
	}

	SSL_set_ex_data(ssl, 0, this);
	
	if (!(read_bio = BIO_new(BIO_s_mem())))
	{
		SSL_free(ssl);
		ssl = nullptr;
		return Error("-DTLSConnection::Init() | Failed to allocate memory for inbound SSL traffic\n");
	}
	
	BIO_set_mem_eof_return(read_bio, -1);

	if (!(write_bio = BIO_new(BIO_s_mem())))
	{
		SSL_free(ssl);
		ssl = nullptr;
		return Error("-DTLSConnection::Init() | Failed to allocate memory for outbound SSL traffic\n");
	}
	BIO_set_mem_eof_return(write_bio, -1);

	//Set MTU of datagrams so it fits in an UDP packet
	SSL_set_mtu(ssl, RTPPAYLOADSIZE);
	//DTLS_set_link_mtu(ssl, MTU);
	BIO_ctrl(write_bio, BIO_CTRL_DGRAM_SET_MTU, RTPPAYLOADSIZE, NULL);

	SSL_set_bio(ssl, read_bio, write_bio);

	switch(setup)
	{
		case SETUP_ACTIVE:
		case SETUP_ACTPASS:
			Debug("-DTLSConnection::Init() | we are SETUP_ACTIVE\n");
			SSL_set_connect_state(ssl);
			break;
		case SETUP_PASSIVE:
			Debug("-DTLSConnection::Init() | we are SETUP_PASSIVE\n");
			SSL_set_accept_state(ssl);
			break;
		case SETUP_HOLDCONN:
		default:
			return Error("-DTLSConnection::Init() | we are hold conn!");
	}
	
	//Now we are ready to read and write DTLS packets.
	inited = true;
	
	//Start handshake
	SSL_do_handshake(ssl);
	
	//Start timeout
	timeout = timeService.CreateTimer(0ms, [this](auto now){
		//UltraDebug("-DTLSConnection::Timeout()\n");
		//Check if still inited
		if (inited)
		{
			//Run timeut
			if (DTLSv1_handle_timeout(ssl)!=-1)
				//Check if there is any pending data
				CheckPending();
		}
	});

	//Set name for debug
	timeout->SetName("DTLSConnection - timeout");
	
	//Start sctp transport
	sctp.OnPendingData([this](){
		//UltraDebug("-sctp::OnPendingData() [ssl:%p]\n",ssl);

		if (ssl)
		{
			BYTE msg[MTU];
			size_t len;
			//Read from sctp transport
			while((len = sctp.ReadPacket(msg,MTU)))
			{
				UltraDebug("-sctp::OnPendingData() [len:%lu]\n",len);
				DumpAsC(msg,len);
				//Write it to the ssl context
				SSL_write(ssl,msg,len);
			}
			
			//Check if there is any pending data
			CheckPending();
		}
	});

	Log("<DTLSConnection::Init()\n");

	return 1;
}

void DTLSConnection::End()
{
	TRACE_EVENT("dtls", "DTLSConnection::End");
	Log("-DTLSConnection::End()\n");
	
	if (!inited)
		return;

	// NOTE: Don't use BIO_free() for write_bio and read_bio as they are
	// automatically freed by SSL_free().
	
	//Not inited anymore
	inited = false;
	
	//Cancel dtls timeout
	if (timeout) timeout->Cancel();

	if (ssl) 
	{
		SSL_free(ssl);
		ssl = NULL;
		read_bio = NULL;
		write_bio = NULL;		
	}
	
}

void DTLSConnection::Shutdown()
{
	//If already ended
	if (!inited || !ssl)
		return;

	Log("-DTLSConnection::Shutdown()\n");

	// If the SSL session is not yet finalized don't bother resetting
	if (!SSL_is_init_finished(ssl))
		return;

	// Send close notify (no need to wait for other peer)
	SSL_shutdown(ssl);

	//Check if we need to send the DTLS shutdown packet
	CheckPending();
}

void DTLSConnection::Reset()
{
	TRACE_EVENT("dtls", "DTLSConnection::Reset");
	Log("-DTLSConnection::Reset()\n");

	if (!inited)
		return;
		
	//Run in event loop thread
	timeService.Sync([this](auto now){
		//Send shutdown
		Shutdown();
	});
}

void DTLSConnection::SetRemoteSetup(Setup remote)
{
	Log("-DTLSConnection::SetRemoteSetup() | [remote:%d]\n", remote);

	if (! DTLSConnection::hasDTLS) {
		Error("no DTLS\n");
		return;
	}

	Setup old = setup;

	switch (remote)
	{
		case SETUP_ACTIVE:
			setup = SETUP_PASSIVE;
			break;
		case SETUP_PASSIVE:
			setup = SETUP_ACTIVE;
			break;
		case SETUP_ACTPASS:
			// We can't respond to an actpass setup with actpass ourselves... so respond with active, as we can initiate connections 
			if (setup == SETUP_ACTPASS)
				setup = SETUP_ACTIVE;
			break;
		case SETUP_HOLDCONN:
			setup = SETUP_HOLDCONN;
			break;
		default:
			// This should never occur... if it does exit early as we don't know what state things are in 
			return;
	}

	// If the setup state did not change we go on as if nothing happened 
	if (old == setup || !ssl)
		return;

	switch (setup)
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
	char* str = tmp;
	char* value;
	int pos = 0;

	while ((value = strsep(&str, ":")) && (pos != (EVP_MAX_MD_SIZE - 1)))
		sscanf(value, "%02x", (unsigned int*)&remoteFingerprint[pos++]);

	free(tmp);
}

int DTLSConnection::Read(BYTE* data,DWORD size)
{
	TRACE_EVENT("dtls", "DTLSConnection::Read", "size", size);
	//UltraDebug("-DTLSConnection::Read() | [pending:%d]\n",BIO_ctrl_pending(write_bio));
	
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
	//Log
	UltraDebug("-DTLSConnection::onSSLInfo() | SSL status: %s [where:%d, ret:%d] | handshake done: %s\n", SSL_state_string_long(ssl), where, ret, SSL_is_init_finished(ssl) ? "yes" : "no");

	if (where & SSL_CB_ALERT) 
	{
		const char* alertType;
		//Get alert type
		switch (*SSL_alert_type_string(ret))
		{
			case 'W':
				alertType = "warning";
				break;

			case 'F':
				alertType = "fatal";
				break;

			default:
				alertType = "undefined";
		}
		//Check direction
		if ((where & SSL_CB_READ) != 0)
			//Log
			Debug("-DTLSConnection::onSSLInfo() | received DTLS %s alert: %s\n", alertType, SSL_alert_desc_string_long(ret));
		else if ((where & SSL_CB_WRITE) != 0)
			//Log
			Debug("-DTLSConnection::onSSLInfo() | sending DTLS %s alert: %s\n", alertType, SSL_alert_desc_string_long(ret));
		else
			//Log
			Debug("-DTLSConnection::onSSLInfo() | DTLS %s alert: %s\n", alertType, SSL_alert_desc_string_long(ret));
	} else if (where & SSL_CB_EXIT) {
		if (ret == 0)
			//Log
			Debug("-DTLSConnection::onSSLInfo() | failed in %s\n", SSL_state_string_long(ssl));
	} else if (where & SSL_CB_HANDSHAKE_START) {
		//Log
		Debug("-DTLSConnection::onSSLInfo() | DTLS handshake starts\n");
	} else if (where & SSL_CB_HANDSHAKE_DONE) {
		//Log
		Log("-DTLSConnection::onSSLInfo() | DTLS handshake done\n");

		// Use the keying material to set up key/salt information 
		if (!SetupSRTP())
			//Error
			listener.onDTLSSetupError();
	} 

	//Check pending data for writing
	CheckPending();
}

int DTLSConnection::Renegotiate()
{
	TRACE_EVENT("dtls", "DTLSConnection::Renegotiate");
	//Run in event loop thread
	timeService.Async([this](auto now){
		if (ssl)
		{

			TRACE_EVENT("dtls", "DTLSConnection::Renegotiate::Work");
			SSL_renegotiate(ssl);
			SSL_do_handshake(ssl);
		}
	});
	
	return 1;
}

int DTLSConnection::SetupSRTP()
{
	TRACE_EVENT("dtls", "DTLSConnection::SetupSRTP");
	if (! DTLSConnection::hasDTLS)
		return Error("-DTLSConnection::SetupSRTP() | no DTLS\n");

	X509* certificate;
	unsigned char fingerprint[EVP_MAX_MD_SIZE];
	unsigned int size=0;
	const EVP_MD* hashFunction;
	std::string hashStr;

	size_t MaxLength = 44;
	BYTE material[MaxLength*2];
	BYTE localMasterKey[MaxLength];
	BYTE remoteMasterKey[MaxLength];
	BYTE *localKey, *localSalt, *remoteKey, *remoteSalt;

	if (!(certificate = SSL_get_peer_certificate(ssl)))
		return Error("-DTLSConnection::SetupSRTP() | no certificate was provided by the peer\n");

	switch (remoteHash)
	{
		case SHA1:
			hashFunction = EVP_sha1();
			hashStr = "SHA-1";
			break;
		case SHA224:
			hashFunction = EVP_sha224();
			hashStr = "SHA-224";
			break;
		case SHA256:
			hashFunction = EVP_sha256();
			hashStr = "SHA-256";
			break;
		case SHA384:
			hashFunction = EVP_sha384();
			hashStr = "SHA-384";
			break;
		case SHA512:
			hashFunction = EVP_sha512();
			hashStr = "SHA-512";
			break;
		default:
			X509_free(certificate);
			return Error("-DTLSConnection::SetupSRTP() | unknown remote hash, cannot verify remote fingerprint\n");
	}

	if (strlen((char*)remoteFingerprint) && (!X509_digest(certificate, hashFunction, fingerprint, &size) || !size || memcmp(fingerprint, remoteFingerprint, size)))
	{
		X509_free(certificate);
		return Error("-DTLSConnection::SetupSRTP() | fingerprint in remote SDP does not match that of peer certificate (hash %s)\n", hashStr.c_str());
	}

	Debug("-DTLSConnection::SetupSRTP() | fingerprint in remote SDP matches that of peer certificate (hash %s)\n", hashStr.c_str());
	X509_free(certificate);

	// Produce key information and set up SRTP 
	if (! SSL_export_keying_material(ssl, material, MaxLength*2, "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0))
		return Error("-DTLSConnection::SetupSRTP() | Unable to extract SRTP keying material from DTLS-SRTP negotiation on RTP instance \n");
	
	//Get negotiated srtp profile
	auto profile = SSL_get_selected_srtp_profile(ssl);
		
	//Check it
	if (!profile)
		return Error("-DTLSConnection::SetupSRTP() | Unable to retrieve negotiated SRTP suite\n");
	
	//Get suite
	Suite suite = SuiteFromName(profile->name);
	
	//Check we know it
	if (suite==UNKNOWN_SUITE)
		return Error("-DTLSConnection::SetupSRTP() | Unknown negotiated SRTP suite [id:%lu,name:'%s']\n",profile->id,profile->name);
	
	//Get key & salt lengths
	auto keysalt = SuiteKeySaltLength(suite);
	size_t total = keysalt.first+keysalt.second;
	
	// Whether we are acting as a server or client determines where the keys/salts are 
	if (setup == SETUP_ACTIVE)
	{
		localKey	= material;
		remoteKey	= localKey  + keysalt.first;
		localSalt	= remoteKey + keysalt.first;
		remoteSalt	= localSalt + keysalt.second;
	} else	{
		remoteKey	= material;
		localKey	= remoteKey  + keysalt.first;
		remoteSalt	= localKey   + keysalt.first;
		localSalt	= remoteSalt + keysalt.second;
	}
	
	//Create local master key
	memcpy(localMasterKey			,localKey	,keysalt.first);
	memcpy(localMasterKey+keysalt.first	,localSalt	,keysalt.second);
	//Create remote master key
	memcpy(remoteMasterKey			,remoteKey	,keysalt.first);
	memcpy(remoteMasterKey+keysalt.first	,remoteSalt	,keysalt.second);

	//Fire event
	listener.onDTLSSetup(suite,localMasterKey,total,remoteMasterKey,total);

	return 1;
}

int DTLSConnection::Write(const BYTE *buffer, DWORD size)
{
	TRACE_EVENT("dtls", "DTLSConnection::Write", "size", size);
	//UltraDebug("-DTLSConnection::Write()\n");
	
	if (!DTLSConnection::hasDTLS)
		return Error("-DTLSConnection::Write() | no DTLS\n");

	if (!inited || !read_bio) 
		return Error("-DTLSConnection::Write() | SSL not yet ready\n");

	BIO_write(read_bio, buffer, size);
	
	//Check pending dtls data for sending
	CheckPending();
	
	BYTE msg[MTU];
	int len = SSL_read(ssl, msg, MTU);
	
	if (len<0)
	{
		int err = SSL_get_error(ssl,len);
		if (err!=SSL_ERROR_WANT_READ)
			return Error("-DTLSConnection::Write() | SSL_read error [ret:%d,err:%d]\n",len,SSL_get_error(ssl,len));
		else 
			return 0;
	}
	//DumpAsC(msg,len);
	Debug("-sctp of len %d\n",len);
	if (len) DumpAsC(msg,len);
	//Pass data to sctp
	if (len && !sctp.WritePacket(msg,len))
		return Error("sctp parse error");

	// Check if the peer sent close alert or a fatal error happened.
	if (SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN)
	{
		Debug("-DTLSConnection::Write() | SSL_RECEIVED_SHUTDOWN on instance '%p', resetting SSL\n", this);
		SSL_clear(ssl);
		//Fire event
		listener.onDTLSShutdown();
		return 0;
	}
	
	//Done
	return 1;
}

void DTLSConnection::CheckPending()
{
	//UltraDebug("-DTLSConnection::CheckPending()\n");
	//Check if there is any pending 
	if (write_bio && BIO_ctrl_pending(write_bio))
		listener.onDTLSPendingData();
	//Reschedule timer
	timeval tv = {};
	if (timeout && DTLSv1_get_timeout(ssl, &tv))
	{
		//GET TIME
		auto ms = getTime(tv) / 1000;
		//IF any
		if (ms)
			//Get 
			timeout->Again(std::chrono::milliseconds(ms));
	}
}

