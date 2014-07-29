/* 
 * File:   cpim.h
 * Author: Sergio
 *
 * Created on 28 de julio de 2014, 13:45
 */

#ifndef CPIM_H
#define	CPIM_H

#include "config.h"
#include "http.h"
#include "amf.h"

class MIMEObject
{
public:
	virtual DWORD Serialize(BYTE* buffer,DWORD size) = 0;
	virtual ~MIMEObject() {};
};

class MIMEText : public std::wstring, public MIMEObject
{
public:
	MIMEText()	{}
	MIMEText(const std::wstring &text) : std::wstring(text)	{}
	
	virtual DWORD Serialize(BYTE* buffer,DWORD size);
	
public:
	static MIMEText* Parse(const BYTE* buffer,DWORD size);
};

class MIMEWrapper
{
protected:
	MIMEWrapper() 
	{
	    contentType = NULL;
	    object = NULL;
	}
public:
	MIMEWrapper(const std::string &type,const std::string &subtype) 
	{
	    contentType = new ContentType(type,subtype);
	    object = NULL;
	}
	
	MIMEWrapper(const std::string &type,const std::string &subtype, MIMEObject* object) 
	{
		contentType = new ContentType(type,subtype);
		this->object = object;
	}
	
	virtual ~MIMEWrapper()
	{
		if (contentType) delete contentType;
		if (object)	 delete object;
	}
	
	DWORD Serialize(BYTE* buffer,DWORD size);
public:	
	 static MIMEWrapper* Parse(const BYTE* data,DWORD size);

protected:    
    ContentType* contentType;
    MIMEObject*  object;
};

class MIMETextWrapper : public MIMEWrapper
{
public:
    MIMETextWrapper() : MIMEWrapper("text","plain")
    {
        //add also charset
        contentType->AddParameter("charset","utf8");
	//Create text
	object = new MIMEText();
    }
    MIMETextWrapper(const std::wstring &text) : MIMEWrapper("text","plain")
    {
        //add also charset
        contentType->AddParameter("charset","utf8");
        //Set text
        object = new MIMEText(text);
    }
    
    void SetText(const std::wstring &text)	{ ((MIMEText*)object)->assign(text);	}
    std::wstring GetText()			{ return *(MIMEText*)object;			}
};

class Address
{
public:
	Address()
	{
		
	}
    
	Address(const Address &addr)
	{
	    this->displayName = addr.GetDisplayName();
	    this->uri = addr.GetURI();
	}

	Address(const std::wstring &display, const std::wstring &uri)
	{
	    this->displayName = display;
	    this->uri = uri;
	}

	Address(const std::wstring &uri)
	{
	    this->uri = uri;
	}
	std::wstring GetDisplayName()   const { return displayName;   }
	std::wstring GetURI()           const { return uri;           }
    
	void SetAddress(const std::wstring &display, const std::wstring &uri)
	{
	    this->displayName = display;
	    this->uri = uri;
	}
    
	DWORD Serialize(BYTE* buffer,DWORD size) const;
	
private:
    std::wstring displayName;
    std::wstring uri;
};

class CPIMMessage
{
public:
    CPIMMessage(const std::wstring &fromURI, const std::wstring &toURI, const std::wstring &text) : from(fromURI), to(toURI)
    {
        mime = new MIMETextWrapper(text);
    }
    ~CPIMMessage()
    {
        if (mime)
            delete(mime);
    }
    
    const Address&	GetFrom()	const { return from;	}
    const Address&	GetTo()		const { return from;	}
    const MIMEWrapper*	GetContent()	const { return mime;	}
   
   DWORD Serialize(BYTE* buffer,DWORD size) const;
public:
    static CPIMMessage* Parse(const BYTE* data,DWORD size);

private:
    CPIMMessage()
    {
        mime = NULL;
    }
private:    
    CPIMMessage(Address fromAddr,Address toAddr,MIMEWrapper *mime) : from(fromAddr), to(toAddr)
    {
        this->mime = mime;
    }

private:
    Address from;
    Address to;
    MIMEWrapper *mime;
};

#endif	/* CPIM_H */

