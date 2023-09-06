#ifndef _AMF_H_
#define _AMF_H_
#include <map>
#include <vector>
#include <string>
#include "config.h"
#include "utf8.h"

class U16Parser
{
public:
	U16Parser();
	void Reset();
	DWORD Parse(BYTE *data,DWORD size);
	bool IsParsed();
	WORD GetValue();
	void SetValue(WORD value);
	DWORD Serialize(BYTE *data,DWORD size);
	DWORD GetSize();
private:
	WORD value = 0;
	WORD len = 0;
};

class U32Parser
{
public:
	U32Parser();
	void Reset();
	DWORD Parse(BYTE *data,DWORD size);
	bool IsParsed();
	DWORD GetValue();
	void SetValue(DWORD value);
	DWORD Serialize(BYTE *data,DWORD size);
	DWORD GetSize();
private:
	DWORD value = 0;
	WORD len = 0;
};

class U29Parser
{
public:
	U29Parser();
	void Reset();
	DWORD Parse(BYTE *data,DWORD size);
	bool IsParsed();
	DWORD GetValue();
private:
	DWORD value = 0;
	WORD len = 0;
	bool last = false;
};



class AMFData
{
public:
	enum ValueType
	{
		Number,
		Boolean,
		String,
		Object,
		MovieClip,
		Null,
		Undefined,
		Reference,
		EcmaArray,
		StrictArray,
		Date,
		LongString,
		Unsupported,
		RecordSet,
		XmlDocument,
		TypedObject
	};

public:
	AMFData();
	virtual ~AMFData();
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
	virtual void Dump();
	virtual AMFData* Clone();
public:
	operator bool();
	operator double();
	operator std::wstring();
protected:
	DWORD len;
};

class AMFParser
{
public:
	enum TypeMarker
	{
		NumberMarker = 0x00,
		BooleanMarker = 0x01,
		StringMarker = 0x02,
		ObjectMarker = 0x03,
		MovieClipMarker = 0x04,
		NullMarker = 0x05,
		UndefinedMarker = 0x06,
		ReferenceMarker = 0x07,
		EcmaArrayMarker = 0x08,
		ObjectEndMarker = 0x09,
		StrictArrayMarker = 0x0A,
		DateMarker = 0x0B,
		LongStringMarker = 0x0C,
		UnsupportedMarker = 0x0D,
		RecordsetMarker = 0x0E,
		XmlDocumentMarker = 0x0F,
		TypedObjectMarker = 0x10,
		AvmplusObjectMarker = 0x11
	};
public:
	AMFParser();
	~AMFParser();
	DWORD Parse(BYTE *data,DWORD size);
	AMFData* GetObject();
	bool IsParsed();
	void Reset();
private:
	AMFData* object;
};

class AMFNumber : public AMFData
{
public:
	AMFNumber();
	AMFNumber(double value) {SetNumber(value);}
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	virtual DWORD GetSize();
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return Number;};
	double GetNumber();
	void SetNumber(double value);
	virtual void Dump();
	virtual AMFData* Clone();
private:
	QWORD value;
};

class AMFBoolean : public AMFData
{
public:
	AMFBoolean();
	AMFBoolean(bool val);
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	virtual DWORD Serialize(BYTE* data,DWORD size);
	virtual DWORD GetSize();
	virtual ValueType GetType() {return Boolean;};
	bool GetBoolean();
	void SetBoolean(bool value);
	virtual AMFData* Clone();
	virtual void Dump();
private:
	bool value;
};

class AMFString : public AMFData
{
public:
	AMFString();
	AMFString(const wchar_t *val);
	AMFString(const std::wstring& val);
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	virtual DWORD GetSize();
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return String;};
	virtual void Reset();
	std::wstring GetWString();
	const wchar_t* GetWChar();
	void SetWString(const std::wstring& value);
	DWORD GetUTF8Size();
	std::string GetUTF8String();
	virtual void Dump();
	virtual AMFData* Clone();
private:
	UTF8Parser utf8parser;
	U16Parser u16parser;
};

class AMFLongString : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return LongString;};
	virtual void Reset();
	std::wstring GetWString();
	std::string GetUTF8String();
	DWORD GetUTF8Size();
private:
	UTF8Parser utf8parser;
	U32Parser u32parser;
};

typedef std::map<std::wstring,AMFData*> AMFObjectMap;

class AMFObject : public AMFData
{
public:
	AMFObject();
	~AMFObject();
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual DWORD GetSize();
	virtual DWORD Serialize(BYTE* buffer,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return Object;};
	AMFObjectMap& GetProperties();
	void AddProperty(const wchar_t* key,const wchar_t* value);
	void AddProperty(const wchar_t* key,const std::wstring& value);
	void AddProperty(const wchar_t* key,const wchar_t* value,DWORD length);
	void AddProperty(const wchar_t* key,const double value);
	void AddProperty(const wchar_t* key,const bool value);
	void AddProperty(const wchar_t* key,AMFData *obj);
	void AddProperty(const wchar_t* key,AMFData &obj);
	AMFData& GetProperty(const wchar_t* key);
	bool HasProperty(const wchar_t* key);
	virtual void Dump();
	virtual AMFData* Clone();
private:
	AMFString key;
	bool marker;
	AMFObjectMap properties;
	std::vector<std::wstring> propertiesOrder;	
	AMFParser parser;
	UTF8Parser utf8parser;
};

class AMFEcmaArray : public AMFData
{
public:
	AMFEcmaArray();
	~AMFEcmaArray();
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual DWORD GetSize();
	virtual ValueType GetType() {return EcmaArray;};
	virtual void Dump();
	AMFObjectMap& GetElements();
	DWORD GetLength();
	void AddProperty(const wchar_t* key,const wchar_t* value);
	void AddProperty(const wchar_t* key,const wchar_t* value,DWORD length);
	void AddProperty(const wchar_t* key,const std::wstring& value);
	void AddProperty(const wchar_t* key,const double value);
	void AddProperty(const wchar_t* key,bool value);
	void AddProperty(const wchar_t* key,AMFData *obj);
	void AddProperty(const wchar_t* key,AMFData &obj);
	AMFData& GetProperty(const wchar_t* key);
	bool HasProperty(const wchar_t* key);
	virtual AMFData* Clone();

private:
	AMFString key;
	AMFObjectMap elements;
	AMFParser parser;
	UTF8Parser utf8parser;
	U32Parser num;
	bool marker;
};

class AMFTypedObject : public AMFObject
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return TypedObject;};
	std::wstring GetClassName();

private:
	AMFParser parser;
	U16Parser u16parser;
	UTF8Parser utf8parser;
};

class AMFStrictArray : public AMFData
{
public:
	AMFStrictArray();
	virtual ~AMFStrictArray();
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return StrictArray;};
	virtual void Dump();
	AMFData** GetElements();
	DWORD GetLength();
private:
	AMFData** elements;
	AMFParser parser;
	U32Parser num;
	DWORD cur;
};

class AMFDate : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() {return Date;};
	time_t& GetDate();
private:
	time_t value {};
};


class AMFMovieClip : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return MovieClip;};
};

class AMFNull : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	virtual DWORD Serialize(BYTE* data,DWORD size);
	virtual ValueType GetType() {return Null;};
};

class AMFUndefined : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return Undefined;};
};

class AMFReference : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed();
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return Reference;};
	WORD GetReference();
private:

	WORD value = 0;
};

class AMFUnsupported : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return Unsupported;};
};

class AMFRecordSet : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed() {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() {return RecordSet;};
};

class AMFXmlDocument : public AMFString
{
public:
	ValueType GetType() {return XmlDocument;};
};


#endif
