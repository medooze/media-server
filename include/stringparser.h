/*
 * File:   stringparser.h
 * Author: Sergio
 *
 * Created on 11 de enero de 2013, 20:50
 */

#ifndef STRINGPARSER_H
#define	STRINGPARSER_H
#include <string>
#include <cstring>
#include <memory>
#include <errno.h>
#include <math.h>
#include <wchar.h>
#include "config.h"
#include "log.h"

template <typename _CharT, typename _StringT>
class  BaseStringParser
{
public:
	BaseStringParser(const _StringT &str)
	{
		buffer = (_CharT*)str.c_str();
		size = str.size();
		c = buffer;
	}

	BaseStringParser(const _CharT* buffer,DWORD size)
	{
		this->buffer = (_CharT*)buffer;
		this->size = size;
		c = this->buffer;
	}
	_CharT* Mark()
	{
		return c;
	}
	_CharT Get()
	{
		return *c;
	}
	void  Reset(_CharT* pos)
	{
		c = pos;
	}

	bool IsEnded()
	{
		return c-buffer==size;
	}

	bool  CheckAlpha()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//ALPHA = %x41-5A / %x61-7A
		return (*c>=0x41 && *c<=0x5A) || (*c>=0x61 && *c<=0x7A);
	}

	bool  CheckAlphaNum()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//ALPHA DIGIT
		return CheckAlpha() || CheckDigit();
	}

	bool  CheckDigit()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//DIGIT  = %x30-39;
		return (*c>=0x30 && *c<=0x39);
	}

	bool  CheckMark()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Check _CharTs
		return CheckCharSet("");
	}

	bool  CheckChar(const _CharT x)
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Check
		return *c==x;
	}

	bool  CheckCharSet(const _CharT* str)
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Get pointer to init of string
		_CharT *i = (_CharT*)str;
		//Get current _CharT
		_CharT cur = *c;
		//Until the end of the string
		while(*i!=0 && !IsEnded())
			//If it is the same _CharT
			if (*(i++)==cur)
				//found
				return true;
		//Not found
		return false;
	}

	bool  ParseWord()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Parse
		return true;
	}

	bool ParseChar(const _CharT x)
	{
		//Check
		if(!CheckChar(x))
			//Not found
			return false;
		//Skip
		Next();
		//founr
		return true;

	}

	bool  ParseQuotedString()
	{
		//Get mark
		_CharT *m = Mark();
		//Check it starts by "
		if (!ParseChar('"'))
			//Error
			return false;
		//Get init
		_CharT *start = c;
		//Any TEXT except "
		while (!IsEnded() && !CheckChar('"'))
		{
			//Check escapes
			if (Left()>1 && *c=='\\' && *(c+1)=='"')
				//Skip both
				Move(2);
			//Check TEXT
			else if (*c>31)
				Next();
			//Break on anything else
			else
				break;
		}
		//Get end
		_CharT* end = c;
		//Check it starts by "
		if (!ParseChar('"'))
		{
			//Rollback to initial position
			Reset(m);
			//Error
			return false;
		}
		//Set value string
		value = _StringT(start,end-start);
		//Everything ok
		return true;
	}

	bool ParseUntilCharset(const _CharT* str)
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Get init
		_CharT *start = c;
		//Wheck it is a not _CharTset
		while (!IsEnded() && !CheckCharSet(str))
			Next();
		//If we don't have any _CharT
		if (start==c)
			//No token found
			return false;
		//Set value string
		value = _StringT(start,c-start);
		//Done
		return true;
	}

	_StringT& GetValue()
	{
		return value;
	}

	void  SkipSpaces()
	{
		//Wheck it is space
		while (!IsEnded() && CheckChar(' '))
			//increase
			Next();
	}

	void  SkipWSpaces()
	{
		//Wheck it is space
		while (!IsEnded() && CheckCharSet(" \t"))
			//increase
			Next();
	}

	void  SkipCharset(const _CharT* set)
	{
		//Wheck it is space
		while (!IsEnded() && CheckCharSet(set))
			//increase
			Next();
	}

	void Next()
	{
		//Check ended
		if (!IsEnded())
			//Move pointerss
			++c;
	}

	bool ParseLine()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Init
		_CharT *start = c;
		//Find CRLF
		while (Left()>1 && (*c!=13 || *(c+1)!=10))
			//increase
			Next();
		//Check if we have found it
		if (Left()==1)
		{
			//rolback
			c = start;
			//No token found
			return false;
		}
		//Set value string
		value = _StringT(start,c-start);
		//Skip CRLF
		Move(2);
		//Done
		return true;
	}

	DWORD Left()
	{
		return size-(c-buffer);
	}

	_StringT GetLeft()
	{
		//Return what's left
		return _StringT(c,size-(c-buffer));
	}

	void Move(DWORD num)
	{
		c +=num;
	}
        
        DWORD GetPos() 
        {
            return c-buffer;
        }
	
	void Dump(const char *msg)
	{
		Debug(msg);
		::Dump((BYTE*)buffer,16>size*sizeof(_CharT)?size*sizeof(_CharT):16);
	}
	
	_CharT* GetBuffer() 
	{
		return buffer;
	}

protected:
	_CharT* buffer;
	DWORD size;
	_CharT* c;
	_StringT value;
};


class StringParser : public BaseStringParser<char,std::string>
{
public:
	StringParser(const std::string& str) : BaseStringParser<char,std::string>(str)
	{
	}

	StringParser(const char* buffer,DWORD size) : BaseStringParser<char,std::string>(buffer,size)
	{
	}

	bool MatchString(const std::string &str)
	{
		return MatchString(str.c_str());
	}

	bool MatchString(const char* str)
	{
		//Get str len
		size_t len = strlen(str);
		//Check we have enought
		if (Left()<len)
			//Not found
			return false;
		//Init
		char *start = c;
		//Compare
		if (strncmp(c,str,len)!=0)
			//Not match
			return false;
		//Move
		Move(len);
		//Set value string
		value = std::string(start,c-start);
		//Found
		return true;
	}

	bool CheckString(const std::string &str)
	{
		return CheckString(str.c_str());
	}

	bool CheckString(const char* str)
	{
		//Get str len
		size_t len = strlen(str);
		//Check we have enought
		if (Left()<len)
			//Not found
			return false;
		//Compare
		if (strncmp(c,str,len)!=0)
			//Not match
			return false;
		//Found
		return true;
	}

	bool CheckInteger()
	{
		return CheckChar('-') || CheckChar('+') || CheckDigit();
	}

	bool ParseInteger()
	{
		if (! CheckInteger())
			return false;

		//Mark
		char* ini = Mark();
		char* end = NULL;

		//Parse long
		number = strtol(ini,&end,10);

		//Check conversion
		if (!number && (errno==EINVAL || errno==ERANGE || ini==end))
			//error
			goto error;

		//Goto end of number
		Move(end-ini);

		//Done
		return true;

	error:
		//Reset pos
		Reset(ini);

		//Error
		return false;
	}
	
	bool  ParseToken()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Get init
		char *start = c;
		
		//Wheck it is a token
		while (!IsEnded() && *c>32 && *c<127 && !CheckCharSet("()<>@,;:\\\"/[]?={}"))
			Next();
		
		//If we don't have any _CharT
		if (start==c)
			//No token found
			return false;
		//Set value string
		value = std::string(start,c-start);
		//Done
		return true;
	}

	long int GetIntegerValue()
	{
		return number;
	}

private:
	long int number = 0;
};

class WideStringParser : public BaseStringParser<wchar_t,std::wstring>
{
public:
	WideStringParser(const std::wstring& str) : BaseStringParser<wchar_t,std::wstring>(str)
	{
	}

	WideStringParser(const wchar_t* buffer,DWORD size) : BaseStringParser<wchar_t,std::wstring>(buffer,size)
	{
	}

	bool MatchString(const std::wstring &str)
	{
		return MatchString(str.c_str());
	}

	bool MatchString(const wchar_t* str)
	{
		//Get str len
		size_t len = wcslen(str);
		//Check we have enought
		if (Left()<len)
			//Not found
			return false;
		//Init
		wchar_t *start = c;
		//Compare
		if (wcsncmp(c,str,len)!=0)
			//Not match
			return false;
		//Move
		Move(len);
		//Set value string
		value = std::wstring(start,c-start);
		//Found
		return true;
	}

	bool CheckString(const std::wstring &str)
	{
		return CheckString(str.c_str());
	}

	bool CheckString(const wchar_t* str)
	{
		//Get str len
		size_t len = wcslen(str);
		//Check we have enought
		if (Left()<len)
			//Not found
			return false;
		//Compare
		if (wcsncmp(c,str,len)!=0)
			//Not match
			return false;
		//Found
		return true;
	}
	
	bool  ParseToken()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Get init
		wchar_t *start = c;
		
		//Wheck it is a token
		while (!IsEnded() && *c>32 && !CheckCharSet(L"()<>@,;:\\\"/[]?={}"))
			Next();
		
		//If we don't have any _CharT
		if (start==c)
			//No token found
			return false;
		//Set value string
		value = std::wstring(start,c-start);
		//Done
		return true;
	}
	
	bool CheckInteger()
	{
		return CheckChar('-') || CheckChar('+') || CheckDigit();
	}
	
	bool ParseInteger()
	{
		if (! CheckInteger())
			return false;

		//Mark
		wchar_t* ini = Mark();
		wchar_t* end = NULL;

		//Parse long
		number = wcstol(ini,&end,10);

		//Check conversion
		if (!number && (errno==EINVAL || errno==ERANGE || ini==end))
			//error
			goto error;

		//Goto end of number
		Move(end-ini);

		//Done
		return true;

	error:
		//Reset pos
		Reset(ini);

		//Error
		return false;
	}
	
	long int GetIntegerValue()
	{
		return number;
	}
private:
	long int number = 0;
};

class JSONValue 
{
public:
	enum Type { String, Number, Array, Object, Null };
public:
	JSONValue(Type type)
	{
		this->type = type;
	}
	virtual ~JSONValue() = default;
	Type GetType() const { return type; }
	
	virtual std::wstring stringify() = 0;
private:
	Type type;
};

class JSONString : public JSONValue, public std::wstring
{
public:
	JSONString(const std::wstring& value) : JSONValue(String), std::wstring(value) {};
	virtual ~JSONString() = default;
	virtual std::wstring stringify() { return std::wstring(L"\"") + *this + L"\""; };
};

class JSONNumber : public JSONValue
{
public:
	JSONNumber(long double value) : JSONValue(Number)
	{
		this->value = value;
	}
	virtual ~JSONNumber() = default;
	virtual std::wstring stringify() { 
		//Get int value
		int64_t integer = value;
		if (fabs(value-integer)<0.0000001)
			return std::to_wstring(integer);
		return std::to_wstring(value); 
	};
	
	long double value;
};


class JSONBoolean : public JSONValue
{
public:
	JSONBoolean(bool value) : JSONValue(Number)
	{
		this->value = value;
	};
	virtual ~JSONBoolean() = default;
	virtual std::wstring stringify() { return value ? L"true":L"false"; };
	bool value;
};

class JSONNull : public JSONValue
{
public:
	JSONNull() : JSONValue(Null) {};
	virtual std::wstring stringify() { return L"null"; };
};


class JSONArray : public JSONValue, public std::vector<std::unique_ptr<JSONValue>>
{
public:
	JSONArray() : JSONValue(Array) {};
	virtual ~JSONArray() = default;
	virtual std::wstring stringify() 
	{
		std::wstring json = L"[\n";
		
		for (auto it=begin(); it!=end(); ++it)
		{
			if (it!=begin())
				json += L",";
			json += (*it)->stringify();
		}
		json += L"]\n";
		
		return json;
	};
	
	JSONValue* index(size_t i)
	{
		return at(i).get();
	}
};

class JSONObject : public JSONValue, public std::map<std::wstring,std::unique_ptr<JSONValue>>
{
public:
	JSONObject() : JSONValue(Object) {};
	virtual ~JSONObject() = default;
	virtual std::wstring stringify() 
	{
		std::wstring json = L"{\n";
		
		for (auto it=begin(); it!=end(); ++it)
		{
			if (it!=begin())
				json += L",\n";
			json += L"\t" + it->first + L":" +it->second->stringify();
		}
		json += L"}\n";
		
		return json;
	};
	
	JSONValue* attr(const std::wstring& key)
	{
		auto it = find(key);
		if (it==end())
			return NULL;
		return it->second.get();
	}
};

class JSONParser : public WideStringParser
{
public:
	JSONParser(const std::wstring& str) : WideStringParser(str)
	{
	}

	JSONParser(const wchar_t* buffer,DWORD size) : WideStringParser(buffer,size)
	{
	}

	void SkipJSONSpaces()
	{
		return SkipCharset(L" \t\r\n");
	}

	bool ParseJSONObjectStart()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return ParseChar('{');
	}

	bool ParseJSONObjectEnd()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return ParseChar('}');
	}

	bool ParseJSONArrayStart()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return ParseChar('[');
	}

	bool ParseComma()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return ParseChar(',');
	}

	bool ParseDoubleDot()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return ParseChar(':');
	}

	bool ParseJSONArrayEnd()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return ParseChar(']');
	}

	bool CheckJSONObject()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return CheckChar('{');
	}

	bool CheckJSONArray()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return CheckChar('[');
	}

	bool CheckJSONString()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return CheckChar('"');
	}

	bool CheckJSONNumber()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return CheckChar('-') || CheckDigit();
	}

	bool CheckJSONTrue()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return CheckString(L"true");
	}

	bool CheckJSONFalse()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return CheckString(L"false");
	}

	bool CheckJSONNull()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return CheckString(L"null");
	}

	bool ParseJSONNumber()
	{
		//Skip WS
		SkipJSONSpaces();

		//Mark
		wchar_t* ini = Mark();
		wchar_t* end = NULL;

		//Parse double
		number  = wcstold(ini,&end);

		//Check conversion
		if (!number && (errno==EINVAL || ini==end))
			//error
			goto error;

		//Goto end of number
		Move(end-ini);

		//Check if exponent is present
		if (CheckCharSet(L"Ee"))
		{
			//Skip
			Next();

			//Get exponent
			long e = wcstod(Mark(),&end);

			//Check conversion
			if (!e && (errno==EINVAL || ini==end))
				//error
				goto error;

			//Goto end of number
			Move(end-ini);

			//Power
			number *= pow(10.0,e);
		}

		//Done
		return true;

	error:
		//Reset pos
		Reset(ini);

		//Error
		return false;
	}

	bool ParseJSONString()
	{
		//Skip WS
		SkipJSONSpaces();

		//Get mark
		wchar_t *m = Mark();
		//Check it starts by "
		if (!ParseChar('"'))
			//Error
			return false;

		//Init value
		value.clear();

		//Any TEXT except "
		while (!IsEnded() && !CheckChar('"'))
		{
			//Check escapes
			if (*c=='\\')
			{
				//Skip
				Next();
				//Check enought
				if (IsEnded())
					//Error
					goto error;
				//Check next
				switch(Get())
				{
					case '"':
						//Add it
						value.push_back('\"');
						//Skip
						Next();
						//Break
						break;
					case '\\':
						//Add it
						value.push_back('\\');
						//Skip
						Next();
						//Break
						break;
					case '/':
						//Add it
						value.push_back('/');
						//Skip
						Next();
						//Break
						break;
					case 'b':
						//Add it
						value.push_back('\b');
						//Skip
						Next();
						//Break
						break;
					case 'f':
						//Add it
						value.push_back('\f');
						//Skip
						Next();
						//Break
						break;
					case 'n':
						//Add it
						value.push_back('\n');
						//Skip
						Next();
						//Break
						break;
					case 'r':
						//Add it
						value.push_back('\r');
						//Skip
						Next();
						//Break
						break;
					case 't':
						//Add it
						value.push_back('\t');
						//Skip
						Next();
						//Break
						break;
					default:
						//Ensure we have enought
						if (Left()<4)
							//error
							goto error;
						//Skip 4
						Move(4);
						//TODO: Parse them
						value.push_back('?');

				}
			}
			//Check TEXT
			else if (*c>31)
			{
				//Add the value
				value.push_back(*c);
				//Move
				Next();
			}
			//Break on anything else
			else
				break;
		}

		//Check it starts by "
		if (!ParseChar('"'))
			//Exit
			goto error;

		//Everything ok
		return true;
	error:
		//Rollback to initial position
		Reset(m);
		//Error
		return false;
	}

	bool ParseJSONTrue()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return MatchString(L"true");
	}

	bool ParseJSONFalse()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return MatchString(L"false");
	}

	bool ParseJSONNull()
	{
		//Skip WS
		SkipJSONSpaces();

		//Check object start
		return MatchString(L"null");
	}

	bool SkipJSONValue()
	{
		return SkipJSONObject() || SkipJSONArray() || ParseJSONString() || ParseJSONNumber() || ParseJSONTrue() || ParseJSONFalse() || ParseJSONNull();
	}

	bool SkipJSONObject()
	{
		//Get init
		wchar_t* ini =  Mark();

		//Check object start
		if (!ParseJSONObjectStart())
			//exit
			goto error;

		//Check end
		if (ParseJSONObjectEnd())
			//Done
			return true;

		do
		{
			//Skip key : value
			if (!ParseJSONString()  || !ParseDoubleDot() || !SkipJSONValue())
				///exit
				goto error;
		//Next object atribute
		} while (ParseComma());

		//Check object end
		if (!ParseJSONObjectEnd())
			//exit
			goto error;
		//OK
		return true;
	error:
		//Move back
		Reset(ini);
		return false;
	}

	bool SkipJSONArray()
	{
		//Get init
		wchar_t *ini =  Mark();

		//Check object start
		if (!ParseJSONArrayStart())
			//exit
			goto error;

		//Check end
		if (ParseJSONArrayEnd())
			//Done
			return true;

		do
		{
			//Skip member
			if (!SkipJSONValue())
				//exit
				goto error;
		//Next object atribute
		} while (ParseComma());

		//OK
		if (!ParseJSONArrayEnd())
			//exit
			goto error;
		//OK
		return true;
	error:
		//Move back
		Reset(ini);
		return false;
	}

	long double GetNumberValue()
	{
		return number;
	}
	
	std::unique_ptr<JSONValue> Parse()
	{
		JSONValue* parsed = NULL;
		
		if (ParseJSONNull())
		{
			//Null value
			parsed = new JSONNull();
		} else if (ParseJSONObjectStart()) {
			//Create new JSON object
			JSONObject* object = new JSONObject();
			
			//Until end
			while(!ParseJSONObjectEnd())
			{
				 std::wstring key;
				 
				//Get name
				 if(!ParseJSONString())
				 {
					delete(object);
					return NULL;
				 }
				 
				 //Get key
				 key = GetValue();

				 //Parse :
				 if (!ParseDoubleDot())
				{
					delete(object);
					return NULL;
				 }

				//Get value
				auto value = Parse();
				 
				 //Check value
				 if (!value)
				{
					delete(object);
					return NULL;
				 }
				 
				 //Add it
				 (*object)[key] = std::move(value);
					 
				 //Skip comma
				 ParseComma();
			}
			
			//Got it
			parsed = object;
		} else if (ParseJSONArrayStart()) {
			//Create new JSON array
			JSONArray* array = new JSONArray();
			
			//Until end
			while(!ParseJSONArrayEnd())
			{
				 auto value = Parse();
				 
				 //Check value
				 if (!value)
				 {
					delete(array);
					return NULL;
				 }
				 
				 //Add it
				 array->push_back(std::move(value));
				 
				 //Skip comma
				 ParseComma();
			}
			//Got it
			parsed = array;
		} else if (ParseJSONString()) {
			//Return value
			parsed = new JSONString(GetValue());
		} else if (ParseJSONNumber()) {
			//Return value
			parsed = new JSONNumber(GetNumberValue());
		} else if (ParseJSONTrue()) {
			//Return value
			parsed = new JSONBoolean(true);
		} else if (CheckJSONFalse()) {
			//Return value
			parsed = new JSONBoolean(false);
		}
		//Return parsed object
		return std::unique_ptr<JSONValue>(parsed);
	}

private:
	long double number = 0.0;
};
#endif	/* STRINGPARSER_H */

