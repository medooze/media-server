#include "utf8.h"

/****************************
 * Parser helper for UTF8
 ****************************/
UTF8Parser::UTF8Parser()
{
	//Reset
	utf8size = -1;
	bytes = 0;
	len = 0;
	w = 0;
}

UTF8Parser::UTF8Parser(const std::wstring& str)
{
	SetWString(str);
}

UTF8Parser::UTF8Parser(const std::string& str)
{
	SetString(str);
}

void UTF8Parser::Reset()
{
	//Remove al string content
	value.clear();
	//Reset number of utf8 bytes stored
	len = 0;
	utf8size = -1;
	bytes = 0;
	w = 0;
}

void UTF8Parser::SetSize(DWORD size)
{
	//Store it
	utf8size = size;
	//Allocate memory for speed
	value.reserve(utf8size);
}

DWORD UTF8Parser::SetString(const std::string& str)
{
	Reset();
	return Parse((BYTE*)str.c_str(),str.size());
}

DWORD UTF8Parser::SetString(const char* str)
{
	Reset();
	return Parse((BYTE*)str,strlen(str));
}

DWORD UTF8Parser::Parse(const BYTE* buffer,DWORD size)
{
	//Get the data still to be copied in the string
	DWORD copy = utf8size-len;

	//Check the size of the remaining data in the string with the
	if (copy>size)
		//Just copy what it is available
		copy = size;

	//Convert UTF-8 to wide char
	for (DWORD i=0;i<copy;i++)
	{
		//Get char byte
		BYTE c = buffer[i];
		
		if (c <= 0x7f)
		{
			//if it is not first byte
			if (bytes)
			{
				//Append decoding error
				value.push_back(L'.');
				//Start againg
				bytes = 0;
			}
			//Append char to string
			value.push_back((wchar_t)c);
		} else if (c <= 0xbf){
			//second/third/etc byte
			if (bytes)
			{
				//Append to wide char byte
				w = ((w << 6)|(c & 0x3f));
				//Decrease the number of bytes to read
				bytes--;
				//If no mor bytes left to append to wchar
				if (bytes == 0)
					//Append 
					value.push_back(w);
			} else {
				//Error
				value.push_back(L'.');
			}
		} else if (c <= 0xdf){
			//2byte sequence start
			bytes = 1;
			w = c & 0x1f;
		} else if (c <= 0xef){
			//3byte sequence start
			bytes = 2;
			w = c & 0x0f;
		} else if (c <= 0xf7){
			//3byte sequence start
			bytes = 3;
			w = c & 0x07;
		} else{
			value.push_back(L'.');
			bytes = 0;
		}
	}

	//Increase parsed data
	len += copy;

	//Return the number of consumed bytes	
	return copy;
}

bool UTF8Parser::IsParsed() const
{
	return (len==utf8size);
}

const std::wstring UTF8Parser::GetWString() const
{
	return value;
}

const wchar_t* UTF8Parser::GetWChar() const
{
	return value.c_str();
}

DWORD UTF8Parser::GetUTF8Size() const
{
	return utf8size;
}

const std::string UTF8Parser::GetUTF8String() const
{
	std::string utf8string;
	utf8string.resize(utf8size);
	Serialize((BYTE*)utf8string.data(),utf8size);
	return utf8string;
}

DWORD UTF8Parser::GetLength() const
{
	return value.length();
}

void UTF8Parser::SetWString(const std::wstring& str)
{
	Reset();
	
	//Set value
	value = str;
	//Calculate UTF8Size again
	utf8size = 0;
	//Iterate chars
	for (DWORD i=0;i<str.length();i++)
	{
		//Get char
		wchar_t w = str[i];
		//Calculate size
		if (w <= 0x7f)
			utf8size++;
		else if (w <= 0x7ff)
			utf8size += 2;
		else if (w <= 0xffff)
			utf8size += 3;
		else 
			utf8size += 4;
	}
}

void UTF8Parser::SetWChar(const wchar_t *buffer,DWORD bufferLen)
{
	//Set value
	value = std::wstring(buffer,bufferLen);
	//Calculate UTF8Size again
	utf8size = 0;
	//Iterate chars
	for (DWORD i=0;i<bufferLen;i++)
	{
		//Get char
		wchar_t w = buffer[i];
		//Calculate size
		if (w <= 0x7f)
			utf8size++;
		else if (w <= 0x7ff)
			utf8size += 2;
		else if (w <= 0xffff)
			utf8size += 3;
		else
			utf8size += 4;
	}
}

DWORD UTF8Parser::Serialize(BYTE* buffer,DWORD size) const
{
	DWORD len=0;

	//check length
	if (size<utf8size)
		//Return error
		return 0;

	//for each wide char
	for (size_t i = 0; i < value.size(); i++)
	{
		//Get wide char
		wchar_t w = value[i];

		if (w <= 0x7f)
		{
			buffer[len++] = (BYTE)w;
		} else if (w <= 0x7ff) {
			buffer[len++] = 0xc0 | ((w >> 6)& 0x1f);
			buffer[len++] = 0x80 | (w & 0x3f);
		} else if (w <= 0xffff) {
			buffer[len++] = 0xe0 | ((w >> 12)& 0x0f);
			buffer[len++] = 0x80 | ((w >> 6) & 0x3f);
			buffer[len++] = 0x80 | (w & 0x3f);
		} else if (w <= 0x10ffff) {
			buffer[len++] = 0xf0 | ((w >> 18)& 0x07);
			buffer[len++] = 0x80 | ((w >> 12) & 0x3f);
			buffer[len++] = 0x80 | ((w >> 6) & 0x3f);
			buffer[len++] = 0x80 | (w & 0x3f);
		} else
			buffer[len++] = '.';
	}

	return len;
}
