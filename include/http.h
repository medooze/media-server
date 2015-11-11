/* 
 * File:   HTTP.h
 * Author: Sergio
 *
 * Created on 3 de enero de 2013, 14:58
 */

#ifndef __HTTP_H_
#define	__HTTP_H_
#include <map>
#include <vector>
#include <string>
#include "config.h"
#include "stringparser.h"

class Parameters :
	protected std::map<std::string,std::vector<std::string> >
{
public:
	bool Parse(StringParser parser);
	void AddParameter(const std::string& key,const std::string& value);
	void AddParameter(const std::string& key);
	bool HasParameter (const std::string& key);
	std::string GetParameter(const std::string& key) const;
	std::vector<std::string> GetParameters(const std::string& key) const;
	std::string ToString() const;
};

class Headers :
	protected std::map<std::string,std::vector<std::string> >
{
public:
	void AddHeader(const std::string& key,const std::string& value);
	void AddHeader(const std::string& key,const int value);
	bool HasHeader (const std::string& key);
	bool ParseHeader(const std::string &line);
	std::string GetHeader(const std::string& key) const;
	int  GetIntHeader(const std::string& key,int defaultValue) const;
	std::vector<std::string> GetHeaders(const std::string& key) const;
	DWORD Serialize(char* buffer,DWORD size) const;
	DWORD GetSize()  const;
	void Dump() const;
};

class ContentDisposition : public Parameters
{
public:
	static ContentDisposition* Parse(const char* buffer,DWORD size);
	static ContentDisposition* Parse(const std::string &type);
public:
	ContentDisposition();
	ContentDisposition(const std::string &type);
	DWORD Serialize(char* buffer,DWORD size);
	DWORD GetSize();
	std::string GetType()		const { return type;	}
private:
	std::string type;
};

class ContentType : public Parameters
{
public:
	static ContentType* Parse(const char* buffer,DWORD size);
	static ContentType* Parse(const std::string &type);
public:
	ContentType();
	ContentType(const std::string &type,const std::string &subtype);
	DWORD Serialize(char* buffer,DWORD size);
	DWORD GetSize();
	std::string GetType()		const { return type;	}
	std::string GetSubType()	const { return subtype;	}
        bool Equals(const std::string &type,const std::string &subtype)
        {
            return this->type.compare(type)==0 && this->subtype.compare(subtype)==0;
        }
	std::string ToString() const;
	ContentType* Clone() const;
private:
	std::string type;
	std::string subtype;
};

class HTTPRequest : public Headers
{
public:
	HTTPRequest(const std::string method,const std::string requestURI,unsigned short httpMajor, unsigned short httpMinor)
	{
		this->requestURI = requestURI;
		this->method = method;
		this->httpMajor = httpMajor;
		this->httpMinor = httpMinor;
	}
	
        void SetHttpMajor(unsigned short httpMajor)     { this->httpMajor = httpMajor;		}
	void SetHttpMinor(unsigned short httpMinor)     { this->httpMinor = httpMinor;		}
        void SetRequestURI(std::string requestURI)	{ this->requestURI = requestURI;        }
        void SetMethod(std::string method)		{ this->method = method;		}

	unsigned short GetHttpMajor() const  { return httpMajor;	}
	unsigned short GetHttpMinor() const  { return httpMinor;	}
	std::string GetRequestURI() const    { return requestURI;	}
        std::string GetMethod() const        { return method;		}
private:
	std::string method;
	std::string requestURI;
	unsigned short httpMajor;
	unsigned short httpMinor;
};

class HTTPResponse : public Headers
{
public:
	HTTPResponse(unsigned short code,const std::string reason,unsigned short httpMajor, unsigned short httpMinor)
	{
		this->code = code;
		this->reason = reason;
		this->httpMajor = httpMajor;
		this->httpMinor = httpMinor;
	}
        unsigned short GetHttpMinor() const     { return httpMinor;     }
        unsigned short GetHttpMajor() const     { return httpMajor;     }
        std::string GetReason() const		{ return reason;	}
        unsigned short GetCode() const		{ return code;		}

	std::string Serialize();
	
private:
	unsigned short code;
	std::string reason;
	unsigned short httpMajor;
	unsigned short httpMinor;
};

#endif	/* __HTTP_H_ */

