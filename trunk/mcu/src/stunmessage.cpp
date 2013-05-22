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
#include <openssl/sha.h>
#include <openssl/hmac.h>

static const BYTE MagicCookie[4] = {0x21,0x12,0xA4,0x42};

STUNMessage::STUNMessage(Type type,Method method,BYTE* transId)
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

STUNMessage* STUNMessage::Parse(BYTE* data,DWORD size)
{
	//Check size
	if (size < 20)
		//It is not
		return NULL;
	//Check first two bits are cero
	if (data[0] & 0xC0)
		//It is not
		return NULL;
	//Check magic cooke
	if (memcmp(MagicCookie,data+4,4))
		//It is not
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
	WORD type = ((data[0] & 0x01) << 1) | ((data[0] & 0x10) >> 4);

	//Create new message
	STUNMessage* msg = new STUNMessage((Type)type,(Method)method,data+8);


	int i = 20;
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

	//Read atributes
	while (i+4<size)
	{
		//Get attribute type
		WORD attrType = get2(data,i);
		WORD attrLen = get2(data,i+2);
		//Check size
		if (size<i+4+attrLen)
			//Skip
			break;
		//Add it
		msg->AddAttribute((Attribute::Type)attrType,data+i+4,attrLen);
		
		//Next
		i = pad32(i+4+attrLen);
	}

	//Return it
	return msg;
}
DWORD STUNMessage::NonAuthenticatedFingerPrint(BYTE* data,DWORD size)
{
	//Get size - FINGERPRINT
	WORD msgSize = GetSize()-8;

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
	i = pad32(i+4+len);

	//REstore length
	set2(data,2,msgSize-20);

	//Calculate crc 32 XOR'ed with the 32-bit value 0x5354554e
	DWORD crc32 = crc32calc.Update(data,i) ^ 0x5354554e;

	//Set fingerprint attribute
	set2(data,i,Attribute::FingerPrint);
	set2(data,i+2,4);
	set4(data,i+4,crc32);

	//INcrease sixe
	i = pad32(i+8);
	
	//Return size
	return i;
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

void  STUNMessage::AddAttribute(Attribute::Type type,BYTE *data,DWORD size)
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
void  STUNMessage::AddXorAddressAttribute(sockaddr_in* addr)
{
	BYTE aux[8];

	//Unused
	aux[0] = 0;
	//Family
	aux[1] = 1;
	//Set port
	memcpy(aux+2,&addr->sin_port,2);
	//Xor it
	aux[2] ^= MagicCookie[0];
	aux[3] ^= MagicCookie[1];
	//Set addres
	memcpy(aux+4,&addr->sin_addr.s_addr,4);
	//Xor it
	aux[4] ^= MagicCookie[0];
	aux[5] ^= MagicCookie[1];
	aux[6] ^= MagicCookie[2];
	aux[7] ^= MagicCookie[3];
	//Add it
	AddAttribute(Attribute::XorMappedAddress,aux,8);
}

STUNMessage* STUNMessage::CreateResponse()
{
	return new STUNMessage(Response,method,transId);
}