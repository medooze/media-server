#ifndef _AMF3_H_
#define _AMF3_H_
#include <map>
#include <string>
#include "config.h"


class AMF3Data
{
public:
	enum ValueType
	{
		Undefined,
		Null,
		True,
		False,
		Integer,
		Double,
		String,
		Data,
		Array,
		Object,
		XmlDocument,
		ByteArray
	};

public:
	AMF3Data();
	virtual ~AMF3Data();
	virtual void Reset();
	virtual DWORD Parse(BYTE* buffer,DWORD size) = 0;
	virtual bool IsParsed() = 0; 
	virtual DWORD Serialize(BYTE* buffer,DWORD size);
	virtual DWORD GetSize();
	virtual ValueType GetType() = 0;
	void AssertType(ValueType type);
	bool CheckType(ValueType type);
	const char* GetTypeName();
	static const char* TypeToString(ValueType type);
public:
	operator bool();
	operator DWORD();
	operator double();
	operator std::wstring&();
protected:
	DWORD len;
};

class AMF3Parser
{
public:
	enum TypeMarker
	{
		UndefinedMarker 	= 0x00,
		NullMarker 		= 0x01,
		FalseMarker 		= 0x02,
		TrueMarker 		= 0x03,
		IntegerMarker 		= 0x04,
		DoubleMarker 		= 0x05,
		StringMarker 		= 0x06,
		DateMarker 		= 0x07,
		ArrayMarker		= 0x08,
		ObjectMarker 		= 0x09,
		XmlDocumentMarker 	= 0x0A,
		ByteArrayMarker 	= 0x0B
	};
public:
	AMF3Parser();
	~AMF3Parser();
	DWORD Parse(BYTE *data,DWORD size);
	AMF3Data* GetObject();
	bool IsParsed();
	void Reset();
private:
	AMF3Data* object;
};

class AMF3Undefined : public AMF3Data
{
public:
	virtual ValueType GetType() {return Undefined;};
};

class AMF3Null : public AMF3Data
{
public:
	virtual ValueType GetType() {return Null;};
};

class AMF3False : public AMF3Data
{
public:
	virtual ValueType GetType() {return False;};
};

class AMF3True : public AMF3Data
{
public:
	virtual ValueType GetType() {return True;};
};


class AMF3Integer : public AMF3Data
{
public:
	AMF3Integer();
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	virtual DWORD GetSize();
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return Number;};
	DWORD GetNumber();
	void SetNumber(DWORD value);
private:
	U29Parser parser;
};


class AMF3String : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	virtual DWORD GetSize();
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return String;};
	virtual void Reset();
	std::wstring& GetWString();
	void SetWString(const std::wstring& value);
	DWORD GetUTF8Size();
private:
	UTF8Parser utf8parser;
	U29 u16parser;
};

class AMF3LongString : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return LongString;};
	virtual void Reset();
	std::wstring& GetWString();
	DWORD GetUTF8Size();
private:
	UTF8Parser utf8parser;
	U32Parser u32parser;
};

typedef std::map<std::wstring,AMF3Data*> AMF3ObjectMap;

class AMF3Object : public AMF3Data
{
public:
	AMF3Object();
	~AMF3Object();
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual DWORD GetSize();
	virtual DWORD Serialize(BYTE* buffer,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return Object;};
	AMF3ObjectMap& GetProperties();
	void AddProperty(const wchar_t* value,const wchar_t* value);
	void AddProperty(const wchar_t* key,const std::wstring& value)
	void AddProperty(const wchar_t* value,const double value);
	void AddProperty(const wchar_t* value,AMF3Object *obj);
	AMF3Data& GetProperty(const wchar_t* key);

private:
	AMF3String key;
	BYTE lenName;
	bool marker;
	AMF3ObjectMap properties;
	AMF3Parser parser;
	UTF8Parser utf8parser;
	WORD utf8size;
};

class AMF3EcmaArray : public AMF3Data
{
public:
	AMF3EcmaArray();
	~AMF3EcmaArray();
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return EcmaArray;};
	AMF3ObjectMap& GetElements();

private:
	BYTE lenName;
	bool marker;
	AMF3ObjectMap elements;
	AMF3Parser parser;
	UTF8Parser utf8parser;
	WORD utf8size;
};

class AMF3TypedObject : public AMF3Object
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return TypedObject;};
	std::wstring& GetClassName();

private:
	AMF3Parser parser;
	U16Parser u16parser;
	UTF8Parser utf8parser;
};

class AMF3StrictArray : public AMF3Data
{
public:
	AMF3StrictArray();
	virtual ~AMF3StrictArray();
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return StrictArray;};
	AMF3Data** GetElements();
private:
	AMF3Data** elements;
	AMF3Parser parser;
	DWORD num;
	DWORD cur;
};

class AMF3Date : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return Date;};
	time_t& GetDate();
private:
	time_t value;
};


class AMF3MovieClip : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return MovieClip;};
};

class AMF3Null : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	virtual DWORD Serialize(BYTE* data,DWORD size);
	virtual ValueType GetType() {return Null;};
};


class AMF3Reference : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return Reference;};
	WORD GetReference();
private:
	WORD value;
};

class AMF3Unsupported : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return Unsupported;};
};

class AMF3RecordSet : public AMF3Data
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return RecordSet;};
};

class AMF3XmlDocument : public AMF3String
{
public:
	ValueType GetType() {return XmlDocument;};
};


#endif
