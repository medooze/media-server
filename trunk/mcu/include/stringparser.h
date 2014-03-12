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
#include "config.h"

template <typename _CharT, typename _StringT>
class  BaseStringParser
{
public:
	BaseStringParser(const _StringT str)
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

	bool  ParseToken()
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Get init
		_CharT *start = c;
		//Wheck it is a token
		while (!IsEnded() && *c>32 && *c<127 && !CheckCharSet("()<>@,;:\\\"/[]?={}"))
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

	bool MatchString(const _StringT &str)
	{
		MatchString(str.c_str());
	}
	
	bool MatchString(const _CharT* str)
	{
		//Get str len
		int len = strlen(str);
		//Check we have enought
		if (Left()<len)
			//Not found
			return false;
		//Init
		_CharT *start = c;
		//Compare
		if (strncmp(c,str,len)!=0)
			//Not match
			return false;
		//Move
		Move(len);
		//Set value string
		value = _StringT(start,c-start);
		//Found
		return true;
	}

	bool CheckString(const _StringT &str)
	{
		CheckString(str.c_str());
	}

	bool CheckString(const _CharT* str)
	{
		//Get str len
		int len = strlen(str);
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

	void Move(DWORD num)
	{
		c +=num;
	}

private:
	_CharT* buffer;
	DWORD size;
	_CharT* c;
	_StringT value;
};


typedef BaseStringParser<char,std::string> StringParser;
typedef BaseStringParser<wchar_t,std::wstring> WideStringParser;
#endif	/* STRINGPARSER_H */

