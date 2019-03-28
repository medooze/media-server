#ifndef _RTMP_HANDSHAKE_H_
#define _RTMP_HANDSHAKE_H_
#include "config.h"
#include <stdexcept>
#include <string.h>
#include <openssl/opensslconf.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include "log.h"


static const uint8_t GenuineFMSKey[] = {
  0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20, 0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x46, 0x6c,
  0x61, 0x73, 0x68, 0x20, 0x4d, 0x65, 0x64, 0x69, 0x61, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
  0x20, 0x30, 0x30, 0x31,	/* Genuine Adobe Flash Media Server 001 */

  0xf0, 0xee, 0xc2, 0x4a, 0x80, 0x68, 0xbe, 0xe8, 0x2e, 0x00, 0xd0, 0xd1, 0x02, 0x9e, 0x7e, 0x57,
  0x6e, 0xec, 0x5d, 0x2d, 0x29, 0x80, 0x6f, 0xab, 0x93, 0xb8, 0xe6, 0x36, 0xcf, 0xeb, 0x31, 0xae
};				/* 68 */

static const uint8_t  GenuineFPKey[] = {
  0x47, 0x65, 0x6E, 0x75, 0x69, 0x6E, 0x65, 0x20, 0x41, 0x64, 0x6F, 0x62, 0x65, 0x20, 0x46, 0x6C,
  0x61, 0x73, 0x68, 0x20, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x20, 0x30, 0x30, 0x31, /* Genuine Adobe Flash Player 001 */
  0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57,
  0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE
};				/* 62 */

static unsigned int GetDigestOffset2(uint8_t *handshake, unsigned int len)
{
	//Check size
	if (len<776)
		//Exit
		return 0;
	//Get offset
	DWORD offset = handshake[772] + handshake[773] + handshake[774] + handshake[775];

	//Calculate result
	return (offset % 728) + 776;
}

static unsigned int GetDigestOffset1(uint8_t *handshake, unsigned int len)
{
	//Check size
	if (len<12)
		//Exit
		return 0;
	//Get offset
	DWORD offset = handshake[8] + handshake[9] + handshake[10] + handshake[11];

	//Calculate result
	return (offset % 728) + 12;
}

static void HMACsha256(const uint8_t *message, size_t messageLen, const uint8_t *key, size_t keylen, uint8_t *digest)
{
	unsigned int digestLen;
	
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  HMAC_CTX ctx;
  HMAC_CTX_init(&ctx); 
	HMAC_Init_ex(&ctx, key, keylen, EVP_sha256(), 0);
	HMAC_Update(&ctx, message, messageLen);
	HMAC_Final(&ctx, digest, &digestLen);
	HMAC_CTX_cleanup(&ctx);
#else
	HMAC_CTX *ctx = HMAC_CTX_new();
	HMAC_Init_ex(ctx, key, keylen, EVP_sha256(), 0);
	HMAC_Update(ctx, message, messageLen);
	HMAC_Final(ctx, digest, &digestLen);
	HMAC_CTX_free(ctx);
#endif

}

static void CalculateDigest(unsigned int digestPos, uint8_t *handshakeMessage, const uint8_t *key, size_t keyLen, uint8_t *digest)
{
	const int messageLen = 1536 - SHA256_DIGEST_LENGTH;
	uint8_t message[1536 - SHA256_DIGEST_LENGTH];

	memcpy(message, handshakeMessage, digestPos);
	memcpy(message + digestPos, &handshakeMessage[digestPos + SHA256_DIGEST_LENGTH], messageLen - digestPos);

	HMACsha256(message, messageLen, key, keyLen, digest);
}

static int VerifyDigest(unsigned int digestPos, uint8_t *handshakeMessage, const uint8_t *key, size_t keyLen)
{
	uint8_t calcDigest[SHA256_DIGEST_LENGTH];
	CalculateDigest(digestPos, handshakeMessage, key, keyLen, calcDigest);
	return memcmp(&handshakeMessage[digestPos], calcDigest, SHA256_DIGEST_LENGTH) == 0;
}

static DWORD GenerateS1Data(BYTE digestOffsetMethod,BYTE *data,DWORD size)
{
      DWORD digestPosServer = 0;
      if (digestOffsetMethod==1)
		digestPosServer = GetDigestOffset1(data, size);
      else
		digestPosServer = GetDigestOffset2(data, size);
      CalculateDigest(digestPosServer, data, GenuineFMSKey, 36, &data[digestPosServer]);
      return digestPosServer;
}

static void GenerateS2Data(BYTE digestOffsetMethod,BYTE *data,DWORD size)
{
	uint8_t digestResp[SHA256_DIGEST_LENGTH];

	DWORD offset = 0;

	//Depending on the initial offset
	if (digestOffsetMethod==1)
		//Get offset of the client message
		offset = GetDigestOffset1(data, size);
	else
		//Get offset of the client message
		offset = GetDigestOffset2(data, size);
	
	//Get the pointer to the client message
	BYTE *clientMessage = data + offset;

	//Calculate digest of the client challenge
	HMACsha256(clientMessage, SHA256_DIGEST_LENGTH, GenuineFMSKey, sizeof(GenuineFMSKey), digestResp);

	//Server signature are the latest 32 bytes
	uint8_t *serverSignature = data+size-SHA256_DIGEST_LENGTH;

	//Generate the signature with the client challenge for the server data
	HMACsha256(data, size-SHA256_DIGEST_LENGTH, digestResp, SHA256_DIGEST_LENGTH, serverSignature);
}

static int VerifyC1Data (BYTE *data,DWORD size)
{
	//Get offset 1
	DWORD offset = GetDigestOffset1(data, size);
	int check = VerifyDigest(offset,data,GenuineFPKey,30);
	Log("-Checked [%d,%d]\n",check,offset);
	if (check)
		return 1;
	//Get offset 2
	offset = GetDigestOffset2(data, size);
	check = VerifyDigest(offset,data,GenuineFPKey,30);
	Log("-Checked [%d,%d]\n",check,offset);
	if (check)
		return 2;
	return 0;

}
static int VerifyC2Data(DWORD digestOffsetMethod,BYTE *data,DWORD size)
{
	uint8_t signature[SHA256_DIGEST_LENGTH];
	uint8_t digest[SHA256_DIGEST_LENGTH];
	DWORD offset = 0;

	//Depending on the initial offset
	if (digestOffsetMethod==1)
		//Get offset of the client message
		offset = GetDigestOffset1(data, size);
	else
		//Get offset of the client message
		offset = GetDigestOffset2(data, size);

	//Get the pointer to the client message
	BYTE *serversig = data + offset;

	HMACsha256(serversig, SHA256_DIGEST_LENGTH, GenuineFPKey, sizeof(GenuineFPKey), digest);

	uint8_t *clientsig = data+size-SHA256_DIGEST_LENGTH;

	HMACsha256(clientsig, size - SHA256_DIGEST_LENGTH, digest, SHA256_DIGEST_LENGTH, signature);

	return (memcmp(signature, &clientsig[size - SHA256_DIGEST_LENGTH],SHA256_DIGEST_LENGTH) != 0);
}
#endif
