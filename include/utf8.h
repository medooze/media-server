/* 
 * File:   utf8.h
 * Author: Sergio
 *
 * Created on 23 de diciembre de 2016, 0:57
 */

#ifndef UTF8_H
#define	UTF8_H
#include <map>
#include <vector>
#include <string>
#include "config.h"

class UTF8Parser
{
public:
	UTF8Parser();
	UTF8Parser(const std::wstring& str);
	UTF8Parser(const std::string& str);
	void Reset();
	DWORD Parse(const BYTE *data,DWORD size);
	bool IsParsed();
	void SetSize(DWORD size);
	std::string GetUTF8String();
	DWORD GetUTF8Size();
	DWORD GetLength();
	std::wstring GetWString();
	const wchar_t* GetWChar();
	void SetWString(const std::wstring& str);
	void SetWChar(const wchar_t* buffer,DWORD bufferLen);
	DWORD SetString(const char* str);
	DWORD SetString(const std::string& str);
	DWORD Serialize(BYTE *data,DWORD size);
private:
	std::wstring value;
	DWORD utf8size;
	DWORD bytes;
	DWORD len;
	wchar_t w;
};

#endif	/* UTF8_H */

