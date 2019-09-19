/* 
 * File:   stunmessage.h
 * Author: Sergio
 *
 * Created on 6 de noviembre de 2012, 15:55
 */

#ifndef STUNMESSAGE_H
#define	STUNMESSAGE_H
#include "config.h"
#include "tools.h"
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class STUNMessage
{
public:
	enum Type  {Request=0,Indication=1,Response=2,Error=3};
	enum Method {Binding=1};

	struct Attribute
	{
		enum Type {
			MappedAddress = 0x0001,
			Username = 0x0006,
			MessageIntegrity = 0x0008,
			ErrorCode = 0x0009,
			UnknownAttributes = 0x000A,
			Realm = 0x0014,
			Nonce = 0x0015,
			Priority = 0x0024,
			UseCandidate = 0x0025,
			XorMappedAddress = 0x0020,
			Software = 0x8022,
			AlternateServer = 0x8023,
			FingerPrint = 0x8028,
			IceControlled = 0x8029,
			IceControlling = 0x802A
		};
		Attribute() = delete;
		Attribute(Attribute&&) = delete;
		Attribute(const Attribute&) = delete;
		Attribute(WORD type,const BYTE *attr,WORD size)
		{
			//Copy values
			this->type = type;
			this->size = size;
			if (attr)
			{
				//allocate
				this->attr = (BYTE*)malloc(size);
				//Copy
				memcpy(this->attr,attr,size);
			} else {
				//Empty
				this->attr = NULL;
			}
		}

		Attribute(WORD type,QWORD data)
		{
			//Copy values
			this->type = type;
			this->size = sizeof(data);
			//allocate
			this->attr = (BYTE*)malloc(size);
			//Copy
			set8(this->attr,0,data);
		}

		Attribute(WORD type,DWORD data)
		{
			//Copy values
			this->type = type;
			this->size = sizeof(data);
			//allocate
			this->attr = (BYTE*)malloc(size);
			//Copy
			set4(this->attr,0,data);
		}

		~Attribute()
		{
			if (attr)
				free(attr);
		}

		Attribute *Clone()
		{
			return new Attribute(type,attr,size);
		}
		WORD type;
		WORD size;
		BYTE *attr;
	};
public:
	static bool IsSTUN(const BYTE* data,DWORD size);
	static STUNMessage* Parse(const BYTE* data,DWORD size);
public:
	STUNMessage(Type type,Method method,const BYTE* transId);
	~STUNMessage();
	STUNMessage* CreateResponse();
	DWORD AuthenticatedFingerPrint(BYTE* data,DWORD size,const char* pwd);
	DWORD NonAuthenticatedFingerPrint(BYTE* data,DWORD size);
	bool CheckAuthenticatedFingerPrint(const BYTE* data,DWORD size,const char* pwd);
	DWORD GetSize();
	Attribute* GetAttribute(Attribute::Type type);
	bool  HasAttribute(Attribute::Type type);
	void  AddAttribute(Attribute* attr);
	void  AddAttribute(Attribute::Type type,const BYTE *data,DWORD size);
	void  AddAttribute(Attribute::Type type,QWORD data);
	void  AddAttribute(Attribute::Type type,DWORD data);
	void  AddAttribute(Attribute::Type type);
	void  AddAddressAttribute(sockaddr_in *addr);
	void  AddXorAddressAttribute(sockaddr_in *addr);
	void  AddXorAddressAttribute(uint32_t addr, uint16_t port);
	void  AddUsernameAttribute(const char* local,const char* remote);

	Type   GetType()		const { return type;	}
	Method GetMethod()		const { return method;	}
	const BYTE*  GetTransactionId()	const { return transId; }
	
	void Dump();
	
private:
	typedef std::vector<Attribute*> Attributes;
private:
	Type	type;
	Method	method;
	BYTE	transId[12];
	Attributes attributes;
};

#endif	/* STUNMESSAGE_H */

