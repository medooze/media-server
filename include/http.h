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
};

class Headers :
	protected std::map<std::string,std::vector<std::string> >
{
public:
	void AddHeader(const std::string& key,const std::string& value);
	bool HasHeader (const std::string& key);
	bool ParseHeader(const std::string &line);
	std::string GetHeader(const std::string& key) const;
	std::vector<std::string> GetHeaders(const std::string& key) const;
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
private:
	std::string type;
	std::string subtype;
};


#endif	/* __HTTP_H_ */

