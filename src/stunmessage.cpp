/*
 * File:   stunmessage.cpp
 * Author: Sergio
 *
 * Created on 6 de noviembre de 2012, 15:55
 */

#include "stunmessage.h"
#include "tools.h"
#include "crc32calc.h"
#include "log.h"
#include <openssl/opensslconf.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>

static const BYTE MagicCookie[4] = {0x21,0x12,0xA4,0x42};

STUNMessage::STUNMessage(Type type,Method method,const BYTE* transId)
{
	//Store values
	this->type = type;
	this->method = method;
	//Copy
	memcpy(this->transId,transId,12);
}

STUNMessage::~STUNMessage()
{
	//For each
	for (Attributes::iterator it = attributes.begin(); it!=attributes.end(); ++it)
		//Delete attr
		delete (*it);
}

bool STUNMessage::IsSTUN(const BYTE* data,DWORD size)
{
	return (
		// STUN headers are 20 bytes.
		(size >= 20) &&
		// First two bits must be zero.
		!(data[0] & 0xC0) &&
		// Magic cookie must match.
		(! memcmp(MagicCookie,data+4,4))
	);
}

STUNMessage* STUNMessage::Parse(const BYTE* data,DWORD size)
{
	//Ensure it looks like a STUN message.
	if (! IsSTUN(data, size))
		return NULL;

	/*
	 * The message type field is decomposed further into the following
	   structure:

				0                 1
				2  3  4 5 6 7 8 9 0 1 2 3 4 5

			       +--+--+-+-+-+-+-+-+-+-+-+-+-+-+
			       |M |M |M|M|M|C|M|M|M|C|M|M|M|M|
			       |11|10|9|8|7|1|6|5|4|0|3|2|1|0|
			       +--+--+-+-+-+-+-+-+-+-+-+-+-+-+

			Figure 3: Format of STUN Message Type Field

	   Here the bits in the message type field are shown as most significant
	   (M11) through least significant (M0).  M11 through M0 represent a 12-
	   bit encoding of the method.  C1 and C0 represent a 2-bit encoding of
	   the class.
	 */
	//Get all bytes
	WORD method = get2(data,0);
	//Normalize
	method =  (method & 0x000f) | ((method & 0x00e0)>>1) | ((method & 0x3E00)>>2);
	//Get class
	WORD type = ((data[0] & 0x01) << 1) | ((data[1] & 0x10) >> 4);

	//Create new message
	STUNMessage* msg = new STUNMessage((Type)type,(Method)method,data+8);

	/*
	  STUN Attributes

	   After the STUN header are zero or more attributes.  Each attribute
	   MUST be TLV encoded, with a 16-bit type, 16-bit length, and value.
	   Each STUN attribute MUST end on a 32-bit boundary.  As mentioned
	   above, all fields in an attribute are transmitted most significant
	   bit first.
	       0                   1                   2                   3
	       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	      |         Type                  |            Length             |
	      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	      |                         Value (variable)                ....
	      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	 */

	//Start looking for attributes after STUN header (byte #20).
	size_t i = 20;

	// Flags (positions) for special MESSAGE-INTEGRITY and FINGERPRINT attributes.
	bool hasMessageIntegrity = false;
	bool hasFingerprint = false;
	DWORD posFingerprint = 0;

	//Read atributes
	//Ensure there are at least 4 remaining bytes (attribute with 0 length).
	while (i+4 <= size)
	{
		//Get attribute type
		WORD attrType = get2(data,i);
		WORD attrLen = get2(data,i+2);

		//Ensure the attribute length is not greater than the remaining size.
		if (size<i+4+attrLen) 
		{
			::Debug("-STUNMessage::Parse() | the attribute length exceeds the remaining size | message discarded\n");
			delete msg;
			return NULL;
		}

		//FINGERPRINT must be the last attribute.
		if (hasFingerprint) 
		{
			::Debug("-STUNMessage::Parse() | attribute after FINGERPRINT is not allowed | message discarded\n");
			delete msg;
			return NULL;
		}

		//After a MESSAGE-INTEGRITY attribute just FINGERPRINT is allowed.
		if (hasMessageIntegrity && attrType != Attribute::FingerPrint) 
		{
			::Debug("-STUNMessage::Parse() | attribute after MESSAGE_INTEGRITY other than FINGERPRINT is not allowed | message discarded\n");
			delete msg;
			return NULL;
		}

		switch(attrType) 
		{
			case Attribute::MessageIntegrity:
				hasMessageIntegrity = true;
				break;
			case Attribute::FingerPrint:
				hasFingerprint = true;
				posFingerprint = i;
				break;
			default:
				break;
		}

		//Add it
		msg->AddAttribute((Attribute::Type)attrType,data+i+4,attrLen);

		//Next
		i = pad32(i+4+attrLen);
	}

	//Ensure current position matches the total length.
	if ((DWORD)i != size) 
	{
		::Debug("-STUNMessage::Parse() | computed message size does not match total size | message discarded\n");
		delete msg;
		return NULL;
	}

	// If it has FINGERPRINT attribute then verify it.
	if (hasFingerprint) 
	{
		// The announced CRC value is 4 bytes long and it is located after 4 bytes
		// from the beginning of the FINGERPRINT attribute.
		// NOTE: Store it into a DWORD variable to avoid endian problems.
		DWORD announced = get4(data, posFingerprint + 4);

		// Compute the CRC32 of the received message up to (but excluding) the
		// FINGERPRINT attribute and XOR it with 0x5354554e.
		CRC32Calc crc32calc;
		DWORD computed = crc32calc.Update(data, posFingerprint) ^ 0x5354554e;

		// Compare them.
		if (announced != computed)
		{
			::Debug("-STUNMessage::Parse() | computed FINGERPRINT value does not match the value in the message | message discarded\n");
			delete msg;
			return NULL;
		}
	}

	//Return it
	return msg;
}
DWORD STUNMessage::NonAuthenticatedFingerPrint(BYTE* data,DWORD size)
{
	//Get size - Message attribute - FINGERPRINT
	WORD msgSize = GetSize()-24-8;

	//Check
	if (size<msgSize)
		//Not enought
		return ::Error("Not enought size");

	//Convert so we can sift
	WORD msgType = type;
	WORD msgMethod = method;

	//Merge the type and method
	DWORD msgTypeField =  (msgMethod & 0x0f80) << 2;
	msgTypeField |= (msgMethod & 0x0070) << 1;
	msgTypeField |= (msgMethod & 0x000f);
	msgTypeField |= (msgType & 0x02) << 7;
	msgTypeField |= (msgType & 0x01) << 4;

	//Set it
	set2(data,0,msgTypeField);

	//Set attributte length
	set2(data,2,msgSize-20);

	//Set cookie
	memcpy(data+4,MagicCookie,4);

	//Set trnasaction
	memcpy(data+8,transId,12);

	DWORD i = 20;

	//For each
	for (Attributes::iterator it = attributes.begin(); it!=attributes.end(); ++it)
	{
		//Set attr type
		set2(data,i,(*it)->type);
		set2(data,i+2,(*it)->size);
		//Check not empty attr
		if ((*it)->attr)
			//Copy
			memcpy(data+i+4,(*it)->attr,(*it)->size);
		//Move
		i = pad32(i+4+(*it)->size);
	}

	//Return size
	return i;
}

DWORD STUNMessage::AuthenticatedFingerPrint(BYTE* data,DWORD size,const char* pwd)
{
	//Get size
	WORD msgSize = GetSize();

	//Check
	if (size<msgSize)
		//Not enought
		return ::Error("Not enought size [size:%u,need:%u\n",size,msgSize);

	//Convert so we can sift
	WORD msgType = type;
	WORD msgMethod = method;

	//Merge the type and method
	DWORD msgTypeField =  (msgMethod & 0x0f80) << 2;
	msgTypeField |= (msgMethod & 0x0070) << 1;
	msgTypeField |= (msgMethod & 0x000f);
	msgTypeField |= (msgType & 0x02) << 7;
	msgTypeField |= (msgType & 0x01) << 4;

	//Set it
	set2(data,0,msgTypeField);

	//Set attributte length
	set2(data,2,msgSize-20);

	//Set cookie
	memcpy(data+4,MagicCookie,4);

	//Set trnasaction
	memcpy(data+8,transId,12);

	DWORD i = 20;

	//For each
	for (Attributes::iterator it = attributes.begin(); it!=attributes.end(); ++it)
	{
		//Set attr type
		set2(data,i,(*it)->type);
		set2(data,i+2,(*it)->size);
		//Check not empty attr
		if ((*it)->attr)
			//Copy
			memcpy(data+i+4,(*it)->attr,(*it)->size);
		//Move
		i = alignMemory4Bytes(data, i+4+(*it)->size);
	}

	DWORD len;
	CRC32Calc crc32calc;

	//Change length to omit the Fingerprint attribute from the HMAC calculation of the message integrity
	set2(data,2,msgSize-20-8);

	//Calculate HMAC and put it in the attibute value
	HMAC(EVP_sha1(),(BYTE*)pwd, strlen(pwd),data,i,data+i+4,&len);

	//Set message integriti attribute
	set2(data,i,Attribute::MessageIntegrity);
	set2(data,i+2,len);

	//INcrease sixe
	i = alignMemory4Bytes(data, i+4+len);

	//REstore length
	set2(data,2,msgSize-20);

	//Calculate crc 32 XOR'ed with the 32-bit value 0x5354554e
	DWORD crc32 = crc32calc.Update(data,i) ^ 0x5354554e;

	//Set fingerprint attribute
	set2(data,i,Attribute::FingerPrint);
	set2(data,i+2,4);
	set4(data,i+4,crc32);

	//INcrease sixe
	i = alignMemory4Bytes(data, i+8);

	//Return size
	return i;
}


bool STUNMessage::CheckAuthenticatedFingerPrint(const BYTE* data,DWORD size,const char* pwd)
{
	//Start looking for attributes after STUN header (byte #20).
	size_t i = 20;

	// Flags (positions) for special MESSAGE-INTEGRITY and FINGERPRINT attributes.
	bool hasMessageIntegrity = false;

	//Read atributes
	//Ensure there are at least 4 remaining bytes (attribute with 0 length).
	while (i+4 <= size)
	{
		//Get attribute type
		WORD attrType = get2(data,i);
		WORD attrLen = get2(data,i+2);

		//Looking for the message integrity attribute position
		if(attrType==Attribute::MessageIntegrity)
		{
			//Found here
			hasMessageIntegrity = true;
			break;
		}

		//Next
		i = pad32(i+4+attrLen);
	}
	
	//Ensure we have found the attribute
	if (!hasMessageIntegrity)
		return false;
	
	//Create new data for authentication and hmac
	BYTE* authentication = (BYTE*)malloc(i+20);
	
	//Copy input
	memcpy(authentication,data,i);
	
	//Change length to add the Fingerprint attribute from the HMAC calculation of the message integrity
	set2(authentication,2,i+4);

	//Calculate HMAC and put it in the attibute value
	DWORD len = 0;
	HMAC(EVP_sha1(),(BYTE*)pwd, strlen(pwd),authentication,i,authentication+i,&len);
	
	//Compare generated hmac with integrity attribute
	bool result = len && memcmp(GetAttribute(Attribute::MessageIntegrity)->attr,authentication+i,len)==0;
	
	//Free mem
	free(authentication);
	
	//Return result
	return result;
}

DWORD STUNMessage::GetSize()
{
	//Base message + Message attribute + FINGERPRINT attribute
	DWORD size = 20+24+8;

	//For each
	for (Attributes::iterator it = attributes.begin(); it!=attributes.end(); ++it)
		//Inc size with padding
		size = pad32(size+4+(*it)->size);

	//Write it
	return size;
}

STUNMessage::Attribute* STUNMessage::GetAttribute(Attribute::Type type)
{
	//For each
	for (Attributes::iterator it = attributes.begin(); it!=attributes.end(); ++it)
		//Check attr
		if ((*it)->type==type)
			//Return it
			return (*it);
	//Not found
	return NULL;
}

bool STUNMessage::HasAttribute(Attribute::Type type)
{
	//For each
	for (Attributes::iterator it = attributes.begin(); it!=attributes.end(); ++it)
		//Check attr
		if ((*it)->type==type)
			//Return it
			return true;
	//Not found
	return false;
}

void  STUNMessage::AddAttribute(Attribute* attr)
{
	//Add it
	attributes.push_back(attr);
}

void  STUNMessage::AddAttribute(Attribute::Type type,const BYTE *data,DWORD size)
{
	//Add it
	attributes.push_back(new Attribute(type,data,size));
}

void  STUNMessage::AddAttribute(Attribute::Type type)
{
	//Add it
	attributes.push_back(new Attribute(type,NULL,0));
}

void  STUNMessage::AddAttribute(Attribute::Type type,QWORD data)
{
	//Add it
	attributes.push_back(new Attribute(type,data));
}

void  STUNMessage::AddAttribute(Attribute::Type type,DWORD data)
{
	//Add it
	attributes.push_back(new Attribute(type,data));
}

void  STUNMessage::AddUsernameAttribute(const char* local,const char* remote)
{
	//Calculate new size
	DWORD size = strlen(local)+strlen(remote)+1;
	//Allocate data
	BYTE* data =(BYTE*)malloc(size+1);
	//Create username
	sprintf((char*)data,"%s:%s",remote,local);
	//Add attribute
	AddAttribute(Attribute::Username,data,size);
	//Free mem
	free(data);
}

void  STUNMessage::AddAddressAttribute(sockaddr_in* addr)
{
	BYTE aux[8];

	//Unused
	aux[0] = 0;
	//Family
	aux[1] = 1;
	//Set port
	memcpy(aux+2,&addr->sin_port,2);
	//Set addres
	memcpy(aux+4,&addr->sin_addr.s_addr,4);
	//Add it
	AddAttribute(Attribute::MappedAddress,aux,8);
}

void  STUNMessage::AddXorAddressAttribute(uint32_t addr, uint16_t port)
{
	BYTE aux[8];

	//Unused
	aux[0] = 0;
	//Family
	aux[1] = 1;
	//Set port
	memcpy(aux+2,&port,2);
	//Xor it
	aux[2] ^= MagicCookie[0];
	aux[3] ^= MagicCookie[1];
	//Set addres
	memcpy(aux+4,&addr,4);
	//Xor it
	aux[4] ^= MagicCookie[0];
	aux[5] ^= MagicCookie[1];
	aux[6] ^= MagicCookie[2];
	aux[7] ^= MagicCookie[3];
	//Add it
	AddAttribute(Attribute::XorMappedAddress,aux,8);
}

void  STUNMessage::AddXorAddressAttribute(sockaddr_in* addr)
{
	AddXorAddressAttribute(addr->sin_addr.s_addr,addr->sin_port);
}

STUNMessage* STUNMessage::CreateResponse()
{
	return new STUNMessage(Response,method,transId);
}

void STUNMessage::Dump()
{
	Debug("[STUNMessage type=%d method=%d]\n",type,method);
	for (const auto attr : attributes)
	{
		Debug("[Attribute type=%d size=%d]\n",attr->type,attr->size);
		::Dump(attr->attr,attr->size);
		Debug("[/Attribute]\n");
	}
	Debug("[/STUNMessage]\n");
}
