/* 
 * File:   multipart.cpp
 * Author: Sergio
 * 
 * Created on 3 de enero de 2013, 14:58
 */
#include <stdio.h>
#include <string.h>
#include "http.h"

bool Parameters::Parse(StringParser parser)
{
	//Skip any whitspace (just in case)
	parser.SkipSpaces();
	//Parse
	while(parser.ParseChar(';'))
	{
		//Skip any whitspace (just in case)
		parser.SkipSpaces();
		//Parse type
		if (!parser.ParseToken())
			//error
			return false;
		//Skip any whitspace (just in case)
		parser.SkipSpaces();
		//Get type
		std::string key = parser.GetValue();

		//Check if we have a value
		if (parser.ParseChar('='))
		{
			//Skip any whitspace (just in case)
			parser.SkipSpaces();
			//Get mark
			char *mark = parser.Mark();
			//Parse value as token
			if (!parser.ParseToken())
			{
				//Rollback
				parser.Reset(mark);
				//Try as a quoted string
				if (!parser.ParseQuotedString())
					//false
					return false;
			}
			//Get type
			std::string value = parser.GetValue();
			//Add parameter
			AddParameter(key,value);
		} else {
			//Add parameter
			AddParameter(key);
		}
	}

	//Good
	return true;
}
void Parameters::AddParameter(const std::string& key,const std::string& value)
{
	//Search
	iterator it = find(key);
	//If found
	if (it!=end())
	{
		//Add
		it->second.push_back(value);
	} else {
		//Create vector
		mapped_type values;
		//Add value
		values.push_back(value);
		//Add to map
		insert(value_type(key,values));
	}
}

void Parameters::AddParameter(const std::string &key)
{
	//Add with empty string
	AddParameter(key,std::string());
}
bool Parameters::HasParameter(const std::string &key)
{
	//Search
	return find(key)!=end();
}

std::string Parameters::GetParameter(const std::string &key) const
{
	//Search
	const_iterator it = find(key);
	//if not found
	if (it==end())
		//return empty string
		return std::string();
	//Return first parameter
	return it->second.front();
}

std::vector<std::string> Parameters::GetParameters(const std::string &key) const
{
	//Search
	const_iterator it = find(key);
	//if not found
	if (it==end())
		//return empty vector
		return std::vector<std::string>();
	//Return parameters
	return it->second;
}

std::string Parameters::ToString() const
{
	//Set size
	std::ostringstream stream;
	//For each key
	for (Parameters::const_iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		std::vector<std::string> values = it->second;
		//For each value
		for (std::vector<std::string>::const_iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//Check if not first
			if (vit!=values.begin())
				//Add ;
				stream << ";";
			//if also got param
			if (!vit->empty())
				//Print
				stream << it->first.c_str() << "=" << vit->c_str();
			else
				//Print
				stream << it->first.c_str();
		}
 	}

	return stream.str();
}

void Headers::AddHeader(const std::string& key,const int value)
{
	std::ostringstream stream;
	//Convert
	stream << value;
	//Add header
	AddHeader(key,stream.str());
}

void Headers::AddHeader(const std::string& key,const std::string& value)
{
	//Search
	iterator it = find(key);
	//If found
	if (it!=end())
	{
		//Add
		it->second.push_back(value);
	} else {
		//Create vector
		mapped_type values;
		//Add value
		values.push_back(value);
		//Add to map
		insert(value_type(key,values));
	}
}

bool Headers::ParseHeader(const std::string &line)
{
	//Create parser
	StringParser parser(line);
	//Parse token
	if (!parser.ParseToken())
		//Error
		return false;
	//Get header
	std::string header = parser.GetValue();
	//Parse :
	if (!parser.ParseChar(':'))
		//Error
		return false;
	//Skip spaces
	parser.SkipWSpaces();
	//Get the rest
	std::string value = parser.GetLeft();
	//Add header
	AddHeader(header,value);
	//Exit
	return true;
}

bool Headers::HasHeader(const std::string &key)
{
	//Search
	return find(key)!=end();
}

std::string Headers::GetHeader(const std::string &key) const
{
	//Search
	const_iterator it = find(key);
	//if not found
	if (it==end())
		//return empty string
		return std::string();
	//Return first Header
	return it->second.front();
}

int  Headers::GetIntHeader(const std::string& key,int defaultValue) const
{
	//Search
	const_iterator it = find(key);
	//if not found
	if (it==end())
		//return default
		return defaultValue;
	//Return first Header
	return atoi(it->second.front().c_str());
}

std::vector<std::string> Headers::GetHeaders(const std::string &key) const
{
	//Search
	const_iterator it = find(key);
	//if not found
	if (it==end())
		//return empty vector
		return std::vector<std::string>();
	//Return Headers
	return it->second;
}

DWORD Headers::Serialize(char* buffer,DWORD size) const
{
	//Check size
	if (size<GetSize())
		//error
		return 0;

	//Set size
	DWORD len = 0;
	//For each key
	for (Headers::const_iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		std::vector<std::string> values = it->second;
		//For each value
		for (std::vector<std::string>::const_iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//if also got param
			if (!vit->empty())
			{
				//Print
				sprintf(buffer+len,"%s: %s\r\n",it->first.c_str(),vit->c_str());
				//add value size and =
				len += it->first.size()+vit->size()+4;
			}
		}
 	}

	return len;
}

void Headers::Dump() const
{
	Debug("[Headers]\r\n");
	//For each key
	for (Headers::const_iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		std::vector<std::string> values = it->second;
		//For each value
		for (std::vector<std::string>::const_iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//if also got param
			if (!vit->empty())
			{
				//Print
				Debug("\t[Header key:%s value:\"%s\"/]\r\n",it->first.c_str(),vit->c_str());
				
			}
		}
 	}
	Debug("[Headers/]\r\n");
}

DWORD Headers::GetSize() const
{
	DWORD size = 0;

	//For each key
	for (Headers::const_iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		std::vector<std::string> values = it->second;
		//For each value
		for (std::vector<std::string>::const_iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//if also got param
			if (!vit->empty())
			{
				//add value size and ': '
				size += it->first.size()+vit->size()+2;
			}
		}
 	}

	return size;
}

/************************
 *
 * HTTP 1.1. ABNF
	Content-Type   = "Content-Type" ":" media-type
	media-type     = type "/" subtype *( ";" parameter )
	type           = token
	subtype        = token
        parameter      = attribute "=" value
	attribute      = token
	value          = token | quoted-string
 *
 */
ContentType* ContentType::Parse(const std::string &type)
{
	return Parse(type.c_str(),type.size());
}

ContentType* ContentType::Parse(const char* buffer,DWORD size)
{
	StringParser parser(buffer,size);

	//Parse type
	if (!parser.ParseToken())
		//Exit
		return NULL;
	//Get type
	std::string type = parser.GetValue();
	//Check "/"
	if(!parser.ParseChar('/'))
		//Exit
		return NULL;
	//Parse subtype
	if (!parser.ParseToken())
		//Exit
		return NULL;
	//Get type
	std::string subtype = parser.GetValue();
	//Create new
	ContentType *contentType = new ContentType(type,subtype);
	//Parse headers
	if (!((Parameters*)contentType)->Parse(parser))
	{
		//Free object
		delete(contentType);
		//Error
		return NULL;
	}
	//return it
	return contentType;
}

ContentType::ContentType()
{
	
}

ContentType::ContentType(const std::string &type,const std::string &subtype)
{
	this->type = type;
	this->subtype = subtype;
}
std::string ContentType::ToString() const
{
	//Set size
	std::ostringstream stream;
	//Create
	stream << type << "/" << subtype;
	//If got params
	if (!Parameters::empty())
		//Append
		stream << ";" << Parameters::ToString();
	//Convert to string and return
	return stream.str();
}

DWORD ContentType::Serialize(char* buffer,DWORD size)
{
	//Check size
	if (size<GetSize())
		//error
		return 0;

	//copy
	sprintf(buffer,"%s/%s",type.c_str(),subtype.c_str());

	//Set size
	DWORD len = type.size()+subtype.size()+1;
	//For each key
	for (Parameters::iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		Parameters::mapped_type& values = it->second;
		//For each value
		for (Parameters::mapped_type::iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//Print
			sprintf(buffer+len,";%s",it->first.c_str());
			//add key size and first ';'
			len += it->first.size()+1;
			//if also got param
			if (!vit->empty())
			{
				//Print
				sprintf(buffer+len,"=%s",vit->c_str());
				//add value size and =
				len += vit->size()+1;
			}
		}
 	}

	return len;
}

ContentType* ContentType::Clone() const
{
	ContentType* cloned =  new ContentType(type,subtype);
	//For each key
	for (Parameters::const_iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		const Parameters::mapped_type& values = it->second;
		//If it is empty
		if (values.begin()==values.end())
			//Add empty param
			cloned->AddParameter(it->first);
		else 
			//For each value
			for (Parameters::mapped_type::const_iterator vit = values.begin(); vit!=values.end(); ++vit)
				//Add it
				cloned->AddParameter(it->first,*vit);
 	}
	//Return it
	return cloned;
}

DWORD ContentType::GetSize()
{
	DWORD size = type.size()+subtype.size()+1;
	
	//For each key
	for (Parameters::iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		Parameters::mapped_type& values = it->second;
		//For each value
		for (Parameters::mapped_type::iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//add key size and first ';'
			size += it->first.size()+1;
			//if also got param
			if (!vit->empty())
				//add value size and =
				size += vit->size()+1;
		}
 	}

	return size;
}





/************************
 *
 * HTTP 1.1. ABNF
	content-disposition = "Content-Disposition" ":" disposition-type *( ";" disposition-parm )
        disposition-type = "attachment" | disp-extension-token
        disposition-parm = filename-parm | disp-extension-parm
        filename-parm = "filename" "=" quoted-string
        disp-extension-token = token
        disp-extension-parm = token "=" ( token | quoted-string )
 *
 */
ContentDisposition* ContentDisposition::Parse(const std::string &type)
{
	return Parse(type.c_str(),type.size());
}

ContentDisposition* ContentDisposition::Parse(const char* buffer,DWORD size)
{
	StringParser parser(buffer,size);

	//Parse type
	if (!parser.ParseToken())
		//Exit
		return NULL;
	//Get type
	std::string type = parser.GetValue();
	//Create new
	ContentDisposition *contentDisposition = new ContentDisposition(type);
	//Parse headers
	if (!((Parameters*)contentDisposition)->Parse(parser))
	{
		//Free object
		delete(contentDisposition);
		//Error
		return NULL;
	}
	//return it
	return contentDisposition;
}

ContentDisposition::ContentDisposition()
{

}

ContentDisposition::ContentDisposition(const std::string &type)
{
	this->type = type;
}

DWORD ContentDisposition::Serialize(char* buffer,DWORD size)
{
	//Check size
	if (size<GetSize())
		//error
		return 0;

	//copy
	sprintf(buffer,"%s",type.c_str());

	//Set size
	DWORD len = type.size()+1;
	//For each key
	for (Parameters::iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		Parameters::mapped_type& values = it->second;
		//For each value
		for (Parameters::mapped_type::iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//Print
			sprintf(buffer+len,";%s",it->first.c_str());
			//add key size and first ';'
			len += it->first.size()+1;
			//if also got param
			if (!vit->empty())
			{
				//Print
				sprintf(buffer+len,"=%s",vit->c_str());
				//add value size and =
				len += vit->size()+1;
			}
		}
 	}

	return len;
}

DWORD ContentDisposition::GetSize()
{
	DWORD size = type.size()+1;

	//For each key
	for (Parameters::iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		Parameters::mapped_type& values = it->second;
		//For each value
		for (Parameters::mapped_type::iterator vit = values.begin(); vit!=values.end(); ++vit)
		{
			//add key size and first ';'
			size += it->first.size()+1;
			//if also got param
			if (!vit->empty())
				//add value size and =
				size += vit->size()+1;
		}
 	}

	return size;
}


std::string HTTPResponse::Serialize()
{
	char line[4096];

	snprintf(line,4096,"HTTP/%d.%d %d %s\r\n",httpMajor,httpMinor,code,reason.c_str());

	//Create response
	std::string response(line);

	///For each header
	for (Headers::iterator it = begin(); it!=end(); ++it)
	{
		//get values vector
		Headers::mapped_type& values = it->second;
		//For each value
		for (Headers::mapped_type::iterator vit = values.begin(); vit!=values.end(); ++vit)
			//Append
			response += it->first +": "+ (*vit) +"\r\n";
 	}

	//Add final \r\n
	response += "\r\n";
	//Return serialized reponse
	return response;
}
