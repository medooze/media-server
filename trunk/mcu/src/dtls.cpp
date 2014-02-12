#include <srtp/srtp.h>
#include "dtls.h"
#include "log.h"

#define DTLS_MASTER_KEY_LEN 16
#define DTLS_MASTER_SALT_LEN 14
#define DTLS_MASTER_LEN (DTLS_MASTER_KEY_LEN + DTLS_MASTER_SALT_LEN)

//Certificates


void dtls_info_callback(const SSL *ssl, int where, int ret)
{
	DTLSConnection *con = (DTLSConnection*) SSL_get_ex_data(ssl, 0);

	/* We only care about alerts */
	if (!(where & SSL_CB_ALERT))
		return;

	con->dtls_failure = 1;
}

std::string DTLSConnection::certfile("mcu.crt");
std::string DTLSConnection::pvtfile("mcy.key");	
std::string DTLSConnection::cipher("ALL:NULL:eNULL:aNULL");

void DTLSConnection::SetCertificate(const char* cert,const char* key)
{
	//Set certificate files
	certfile.assign(cert);
	pvtfile.assign(key);
}

std::string DTLSConnection::GetCertificateFingerPrint(Hash hash)
{
	BIO *certbio;
	X509 *cert;
	unsigned int size;
	char local_fingerprint[EVP_MAX_MD_SIZE*3+1] = {0};
	unsigned char fingerprint[EVP_MAX_MD_SIZE];

	//Creaate BIO for certificate file
	certbio = BIO_new(BIO_s_file());

	//Read certificate filename
	if (!BIO_read_filename(certbio, certfile.c_str()))
	{
		//Error
		Error("Could not read certificate filename [%s]\n",certfile.c_str());
		//Error
		goto end;
	}

	//Read certificate in X509 format
	cert = PEM_read_bio_X509(certbio, NULL, 0, NULL);

	//Check it
	if (!cert)
	{
		//Error
		Error("Could not read X509 certificate from filename [%s]\n",certfile.c_str());
		//Error
		goto end;
	}

	//Check hash algorithm
	switch (hash)
	{
		case SHA1:
			//Hash
			X509_digest(cert, EVP_sha(), fingerprint, &size);
			break;
		case SHA256:
			X509_digest(cert, EVP_sha256(), fingerprint, &size);
	}

	//Check size
	if (!size)
	{
		//Error
		Error("Wrong X509 certificate size\n");
		//Error
		goto end;
	}

	//Convert to hex format
	for (int i = 0; i < size; i++)
		sprintf(local_fingerprint+i*3, "%.2X:", fingerprint[i]);

	//End string
	local_fingerprint[size*3-1] = 0;
end:
	BIO_free_all(certbio);

	return std::string(local_fingerprint);
}

DTLSConnection::DTLSConnection(Listener& listener) : listener(listener)
{
	//Set default values
	rekey		= 0;
	dtls_setup	= SETUP_PASSIVE;
	connection	= CONNECTION_NEW;
	suite		= AES_CM_128_HMAC_SHA1_80;
	dtls_failure	=  false;		/*!< Failure occurred during DTLS negotiation */
	ssl_ctx		= NULL;			/*!< SSL context */
	ssl		= NULL;			/*!< SSL session */
	read_bio	= NULL;			/*!< Memory buffer for reading */
	write_bio	= NULL;			/*!< Memory buffer for writing */
	//Init library just in case
	SSL_library_init();

	//Create ssl context
	ssl_ctx = SSL_CTX_new(DTLSv1_method());
	//Check
	if(!ssl_ctx)
		//Print SSl error
		ERR_print_errors_fp(stderr);

}

DTLSConnection::~DTLSConnection()
{
	//ENd
	End();
	
	if (ssl_ctx)
	{
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
	}
}

int DTLSConnection::SetSuite(Suite suite)
{
	switch(suite)
	{
		case AES_CM_128_HMAC_SHA1_80:
			SSL_CTX_set_tlsext_use_srtp(ssl_ctx, "SRTP_AES128_CM_SHA1_80");
			break;
		case AES_CM_128_HMAC_SHA1_32:
			SSL_CTX_set_tlsext_use_srtp(ssl_ctx, "SRTP_AES128_CM_SHA1_32");
			break;
		default:
			return Error("Unsupported suite [%d] specified for DTLS-SRTP\n",suite);
	}

	return 1;
}

int DTLSConnection::Init()
{
	Log("-DTLS init\n");
	
	//Check
	if(!ssl_ctx)
		//Fail
		return Error("-No SSL contecx\n");
	
	//Verify always
	bool verify = false;

	//Set verify
	SSL_CTX_set_verify(ssl_ctx, verify ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT : SSL_VERIFY_NONE, NULL);

	//Set certifigate
	if (!SSL_CTX_use_certificate_file(ssl_ctx, certfile.c_str(), SSL_FILETYPE_PEM))
		return Error("Specified certificate file '%s' for  could not be used\n", certfile.c_str());

	if (!SSL_CTX_use_PrivateKey_file(ssl_ctx, pvtfile.c_str(), SSL_FILETYPE_PEM) || !SSL_CTX_check_private_key(ssl_ctx))
		return Error("Specified private key file '%s' for  could not be used\n",pvtfile.c_str());

	if (!SSL_CTX_set_cipher_list(ssl_ctx, cipher.c_str()))
		return Error("Invalid cipher specified in cipher list '%s' for \n",cipher.c_str());

/*
 * 	if (cafile || capath)
		if (!SSL_CTX_load_verify_locations(ssl_ctx, cafile, capath))
			return Error("Invalid certificate authority file '%s' or path '%s' specified for \n", cafile?cafile:"(NULL)", capath?capath:"(NULL)");
 */

	if (!(ssl = SSL_new(ssl_ctx)))
		return Error("Failed to allocate memory for SSL context on \n");

	SSL_set_ex_data(ssl, 0, this);
	SSL_set_info_callback(ssl, dtls_info_callback);

	if (!(read_bio = BIO_new(BIO_s_mem())))
		return Error("Failed to allocate memory for inbound SSL traffic on \n");

	BIO_set_mem_eof_return(read_bio, -1);

	if (!(write_bio = BIO_new(BIO_s_mem())))
		return Error("Failed to allocate memory for outbound SSL traffic on \n");
	
	BIO_set_mem_eof_return(write_bio, -1);

	SSL_set_bio(ssl, read_bio, write_bio);

	switch (dtls_setup)
	{
		case SETUP_ACTIVE:
			SSL_set_connect_state(ssl);
			break;
		case SETUP_PASSIVE:
			SSL_set_accept_state(ssl);
			break;
	}

	//New connection
	connection = CONNECTION_NEW;

	//Start handshake
	SSL_do_handshake(ssl);

	return 1;
}


int DTLSConnection::End()
{
	//REset
	//Reset();
	
	if (read_bio)
	{
		//BIO_free(read_bio);
		read_bio = NULL;
	}

	if (write_bio)
	{
		//BIO_free(write_bio);
		write_bio = NULL;
	}

	if (ssl)
	{
	//	SSL_free(ssl);
		ssl = NULL;
	}

	return 1;
}

void DTLSConnection::Reset()
{
	/* If the SSL session is not yet finalized don't bother resetting */
	if (!SSL_is_init_finished(ssl))
		return;

	SSL_shutdown(ssl);
	
	connection = CONNECTION_NEW;
}

void DTLSConnection::SetRemoteSetup(Setup remote)
{
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
			SSL_set_connect_state(ssl);
			break;
		case SETUP_PASSIVE:
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
	//TODO: store hash!
	char *tmp = strdupa(fingerprint), *value;
	int pos = 0;
	
	while ((value = strsep(&tmp, ":")) && (pos != (EVP_MAX_MD_SIZE - 1)))
		sscanf(value, "%02x", (unsigned int*) &remote_fingerprint[pos++]);

	free(tmp);
}

int DTLSConnection::Read(BYTE* data,int size)
{
	if (BIO_ctrl_pending(write_bio))
		return BIO_read(write_bio, data, size);
	return 0;
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
	BYTE material[DTLS_MASTER_LEN * 2];
	BYTE localMasterKey[DTLS_MASTER_LEN];
	BYTE remoteMasterKey[DTLS_MASTER_LEN];
	BYTE *local_key, *local_salt, *remote_key, *remote_salt;

	/* If a fingerprint is present in the SDP make sure that the peer certificate matches it */
	if (SSL_CTX_get_verify_mode(ssl_ctx) != SSL_VERIFY_NONE)
	{
		X509 *certificate;

		if (!(certificate = SSL_get_peer_certificate(ssl)))
			return Error("No certificate was provided by the peer\n");

		/* If a fingerprint is present in the SDP make sure that the peer certificate matches it */
		if (remote_fingerprint[0])
		{
			unsigned char fingerprint[EVP_MAX_MD_SIZE];
			unsigned int size;

			if (!X509_digest(certificate, EVP_sha1(), fingerprint, &size) ||
				!size ||
				memcmp(fingerprint, remote_fingerprint, size))
			{
				X509_free(certificate);
				return Error("Fingerprint provided by remote party does not match that of peer certificate\n");
			}
		}

		X509_free(certificate);
	}

	/* Ensure that certificate verification was successful */
	if (SSL_get_verify_result(ssl) != X509_V_OK)
		return Error("Peer certificate on  failed verification test\n");

	/* Produce key information and set up SRTP */
	if (!SSL_export_keying_material(ssl, material, DTLS_MASTER_LEN * 2, "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0))
		return Error("Unable to extract SRTP keying material from DTLS-SRTP negotiation on RTP instance \n");

	/* Whether we are acting as a server or client determines where the keys/salts are */
	if (dtls_setup == SETUP_ACTIVE)
	{
		local_key = material;
		remote_key = local_key + DTLS_MASTER_KEY_LEN;
		local_salt = remote_key + DTLS_MASTER_KEY_LEN;
		remote_salt = local_salt + DTLS_MASTER_SALT_LEN;
	} else	{
		remote_key = material;
		local_key = remote_key + DTLS_MASTER_KEY_LEN;
		remote_salt = local_key + DTLS_MASTER_KEY_LEN;
		local_salt = remote_salt + DTLS_MASTER_SALT_LEN;
	}

	//Create local master key
	memcpy(localMasterKey,local_key,DTLS_MASTER_KEY_LEN);
	memcpy(localMasterKey+DTLS_MASTER_KEY_LEN,local_salt,DTLS_MASTER_SALT_LEN);
	//Create remote master key
	memcpy(remoteMasterKey,remote_key,DTLS_MASTER_KEY_LEN);
	memcpy(remoteMasterKey+DTLS_MASTER_KEY_LEN,remote_salt,DTLS_MASTER_SALT_LEN);

	//Fire event
	listener.onDTLSSetup(suite,localMasterKey,DTLS_MASTER_LEN,remoteMasterKey,DTLS_MASTER_LEN);

	return 0;
}

int DTLSConnection::Write(BYTE *buffer,int size)
{
	if(!ssl || ! read_bio)
		return Error("DTLS not yet initialized\n");
	
	BIO_write(read_bio, buffer, size);

	SSL_read(ssl, buffer, size);

	if (dtls_failure)
		return Error("DTLS failure occurred on RTP instance '%p', terminating\n",this);

	if (SSL_is_init_finished(ssl))
	{
		/* Any further connections will be existing since this is now established */
		connection = CONNECTION_EXISTING;

		/* Use the keying material to set up key/salt information */
		SetupSRTP();
	}

	return 1;
}

int DTLSConnection::CheckPending()
{
	if (!write_bio)
		return 0;
	
        return BIO_ctrl_pending(write_bio);
}
