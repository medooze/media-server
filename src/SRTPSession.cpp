#include "tracing.h"
#include "SRTPSession.h"
#include <string.h>
#include <algorithm>
#include <arpa/inet.h>
#include "log.h"

SRTPSession::~SRTPSession()
{
	Reset();
}
void SRTPSession::Reset()
{
	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));
	
	//If setup
	if (srtp)
	{
		//Dealloc srtp session
		srtp_dealloc(srtp);
		srtp = nullptr;
	}
}

bool SRTPSession::Setup(const char* suite,const uint8_t* key,const size_t len)
{
	//Reset first
	Reset();

	//Get cypher
	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
// Ignore coverity error: "srtp_crypto_policy_set_rtp_default" in "srtp_crypto_policy_set_rtp_default(&this->policy.rtcp)" looks like a copy-paste error.
// coverity[copy_paste_error]
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_256_GCM")==0) {
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_128_GCM")==0) {
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
	} else {
		//Error
		Error("-SRTPSession::Setup() | Unknown suite [%s]\n",suite);
		//Set error
		err = Status::BadParam;
		return false;
	}

	//Check sizes
	if (len!=(size_t)policy.rtp.cipher_key_len)
	{
		//Error
		Error("-SRTPSession::Setup() | Could not create srtp session wrong key size[got:%llu,needed:%d]\n",len,policy.rtp.cipher_key_len);
		//Set error
		err = Status::BadParam;
		return false;
	}
	
	//Store key
	this->key.assign(key, key+len);
	
	//Set polciy values
	policy.ssrc.type	= ssrc_any_outbound;
	policy.ssrc.value	= 0;
	policy.allow_repeat_tx  = 1;
	policy.window_size	= 1024;
	policy.key		= this->key.data();
	policy.next		= nullptr;
	
	//Create new empty
	err = (Status)srtp_create(&srtp,&policy);

	//Check error
	if (err!=Status::OK)
	{
		//Error
		Error("-SRTPSession::Setup() | Could not create srtp session[%s]\n",GetLastError());
		//Set error
		//No session
		srtp = nullptr;
		//Error
		return false;
	}
	//Add all pending ssrcs now
	for (auto ssrc : pending)
		//Add it
		AddStream(ssrc);
	
	//Clear pending ssrcs
	pending.clear();
		
	//Evrything ok
	return true;
}

void SRTPSession::AddStream(uint32_t ssrc)
{
	//If not setup yet
	if (!srtp)
	{
		//Just push to pending
		pending.push_back(ssrc);
		return;
	}
	
	//Set polciy values
	policy.ssrc.type	= ssrc_specific;
	policy.ssrc.value	= ssrc;
	policy.allow_repeat_tx  = 1;
	policy.window_size	= 1024;
	policy.key		= key.data();
	policy.next		= nullptr;
	
	//Remove it first to clean ROC just in case
	srtp_remove_stream(srtp, htonl(ssrc));
	//Add it
	err = (Status)srtp_add_stream(srtp, &policy);
	
	Log("-SRTPSession::AddStream() | [ssrc:%u,%s]\n",ssrc,GetLastError());
}

void SRTPSession::RemoveStream(uint32_t ssrc)
{
	//If not setup yet
	if (!srtp)
	{
		//Just remove from pending
		pending.erase(std::remove(pending.begin(), pending.end(), ssrc), pending.end());
		return;
	}
	
	//Remove from srtp
	err = (Status)srtp_remove_stream(srtp, htonl(ssrc));
	
	Log("-SRTPSession::RemoveStream() | [ssrc:%u,%s]\n",ssrc,GetLastError());
}
		
size_t SRTPSession::ProtectRTP(uint8_t* data, size_t size)
{
	TRACE_EVENT("srtp", "SRTPSession::ProtectRTP", "size", size);
	int len = size;
	err = (Status)srtp_protect(srtp,(uint8_t*)data,&len);
	return err == Status::OK && len > 0 ? static_cast<size_t>(len) : 0;
}

size_t SRTPSession::ProtectRTCP(uint8_t* data, size_t size)
{
	TRACE_EVENT("srtp", "SRTPSession::ProtectRTCP", "size", size);
	int len = size;
	err = (Status)srtp_protect_rtcp(srtp,(uint8_t*)data,&len);
	return err==Status::OK && len>0 ? static_cast<size_t>(len) : 0;
}

size_t SRTPSession::UnprotectRTP(uint8_t* data, size_t size)
{
	TRACE_EVENT("srtp", "SRTPSession::UnprotectRTP", "size", size);
	int len = size;
	err = (Status)srtp_unprotect(srtp,(uint8_t*)data,&len);
	return err==Status::OK && len>0 ? len : 0;
}


size_t SRTPSession::UnprotectRTCP(uint8_t* data, size_t size)
{
	TRACE_EVENT("srtp", "SRTPSession::UnprotectRTCP", "size", size);
	int len = size;
	err = (Status)srtp_unprotect_rtcp(srtp,(uint8_t*)data,&len);
	return err == Status::OK && len > 0 ? static_cast<size_t>(len) : 0;
}


