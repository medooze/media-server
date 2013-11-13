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

class StringParser
{
public:
	StringParser(const std::string str)
	{
		buffer = (char*)str.c_str();
		size = str.size();
		c = buffer;
	}
	
	StringParser(const char* buffer,DWORD size)
	{
		this->buffer = (char*)buffer;
		this->size = size;
		c = this->buffer;
	}
	char* Mark()
	{
		return c;
	}
	void  Reset(char* pos)
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
		//Check chars
		return CheckCharSet("");
	}

	bool  CheckChar(const char x)
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Check
		return *c==x;
	}

	bool  CheckCharSet(const char* str)
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Get pointer to init of string
		char *i = (char*)str;
		//Get current char
		char cur = *c;
		//Until the end of the string
		while(*i!=0 && !IsEnded())
			//If it is the same char
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

	bool ParseChar(const char x)
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
		char *start = c;
		//Wheck it is a token
		while (!IsEnded() && *c>32 && *c<127 && !CheckCharSet("()<>@,;:\\\"/[]?={}"))
			Next();
		//If we don't have any char
		if (start==c)
			//No token found
			return false;
		//Set value string
		value = std::string(start,c-start);
		//Done
		return true;
	}

	bool  ParseQuotedString()
	{
		//Get mark
		char *m = Mark();
		//Check it starts by "
		if (!ParseChar('"'))
			//Error
			return false;
		//Get init
		char *start = c;
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
		char* end = c;
		//Check it starts by "
		if (!ParseChar('"'))
		{
			//Rollback to initial position
			Reset(m);
			//Error
			return false;
		}
		//Set value string
		value = std::string(start,end-start);
		//Everything ok
		return true;
	}
	
	bool ParseUntilCharset(const char* str)
	{
		//Check ended
		if (IsEnded())
			//Not found
			return false;
		//Get init
		char *start = c;
		//Wheck it is a not charset
		while (!IsEnded() && !CheckCharSet(str))
			Next();
		//If we don't have any char
		if (start==c)
			//No token found
			return false;
		//Set value string
		value = std::string(start,c-start);
		//Done
		return true;
	}

	std::string& GetValue()
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
		char *start = c;
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
		value = std::string(start,c-start);
		//Skip CRLF
		Move(2);
		//Done
		return true;
	}

	DWORD Left()
	{
		return size-(c-buffer);
	}

	std::string GetLeft()
	{
		//Return what's left
		return std::string(c,size-(c-buffer));
	}

	bool MatchString(const std::string &str)
	{
		MatchString(str.c_str());
	}
	
	bool MatchString(const char* str)
	{
		//Get str len
		int len = strlen(str);
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
		CheckString(str.c_str());
	}

	bool CheckString(const char* str)
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
	char* buffer;
	DWORD size;
	char* c;
	std::string value;
};

#endif	/* STRINGPARSER_H */

