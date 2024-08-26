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
	bool IsParsed() const;
	WORD GetValue() const;
	void SetValue(WORD value);
	DWORD Serialize(BYTE *data,DWORD size);
	DWORD GetSize() const;
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
	bool IsParsed() const;
	DWORD GetValue() const;
	void SetValue(DWORD value);
	DWORD Serialize(BYTE *data,DWORD size);
	DWORD GetSize() const;
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
	bool IsParsed() const;
	DWORD GetValue() const;
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
	virtual bool IsParsed() const = 0 ; 
	virtual DWORD Serialize(BYTE* buffer,DWORD size);
	virtual DWORD GetSize() const;
	virtual ValueType GetType() const = 0;
	void AssertType(ValueType type) const;
	bool CheckType(ValueType type)  const;
	const char* GetTypeName() const;
	static const char* TypeToString(ValueType type);
	virtual void Dump() const;
	virtual AMFData* Clone() const;
public:
	operator bool() const;
	operator double() const;
	operator std::wstring() const;
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
	bool IsParsed() const;
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
	virtual bool IsParsed() const;
	virtual DWORD GetSize() const;
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType()  const {return Number;};
	double GetNumber() const;
	void SetNumber(double value);
	virtual void Dump() const;
	virtual AMFData* Clone() const;
private:
	QWORD value;
};

class AMFBoolean : public AMFData
{
public:
	AMFBoolean();
	AMFBoolean(bool val);
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed() const;
	virtual DWORD Serialize(BYTE* data,DWORD size);
	virtual DWORD GetSize() const;
	virtual ValueType GetType()  const {return Boolean;};
	bool GetBoolean() const;
	void SetBoolean(bool value);
	virtual AMFData* Clone() const;
	virtual void Dump() const;
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
	virtual bool IsParsed() const;
	virtual DWORD GetSize() const;
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType()  const {return String;};
	virtual void Reset();
	std::wstring GetWString() const;
	const wchar_t* GetWChar() const;
	void SetWString(const std::wstring& value);
	DWORD GetUTF8Size() const;
	std::string GetUTF8String() const;
	virtual void Dump() const;
	virtual AMFData* Clone() const;
private:
	UTF8Parser utf8parser;
	U16Parser u16parser;
};

class AMFLongString : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed() const;
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() const {return LongString;};
	virtual void Reset();
	std::wstring GetWString() const;
	std::string GetUTF8String() const;
	DWORD GetUTF8Size() const;
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
	virtual DWORD GetSize() const;
	virtual DWORD Serialize(BYTE* buffer,DWORD size);
	virtual bool IsParsed() const;
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType()  const {return Object;};
	AMFObjectMap& GetProperties();
	void AddProperty(const wchar_t* key, const wchar_t* value);
	void AddProperty(const wchar_t* key, const std::wstring& value);
	void AddProperty(const wchar_t* key, const wchar_t* value, DWORD length);
	void AddProperty(const wchar_t* key, const double value);
	void AddProperty(const wchar_t* key, const bool value);
	void AddProperty(const wchar_t* key, AMFData *obj);
	void AddProperty(const wchar_t* key, const AMFData &obj);
	AMFData& GetProperty(const wchar_t* key);
	bool HasProperty(const wchar_t* key) const;
	virtual void Dump() const;
	virtual AMFData* Clone() const;

private:
	// Prevent future problems with shallow copies for now by preventing them
	// Maybe later we will support deep copying of "elements"
	AMFObject(const AMFObject& rhs) = delete;
	AMFObject& operator=(const AMFObject& rhs) = delete;

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
	virtual bool IsParsed() const;
	virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual DWORD GetSize() const;
	virtual ValueType GetType()  const {return EcmaArray;};
	virtual void Dump() const;
	AMFObjectMap& GetElements();
	DWORD GetLength() const;
	void AddProperty(const wchar_t* key, const wchar_t* value);
	void AddProperty(const wchar_t* key, const wchar_t* value,DWORD length);
	void AddProperty(const wchar_t* key, const std::wstring& value);
	void AddProperty(const wchar_t* key, const double value);
	void AddProperty(const wchar_t* key, bool value);
	void AddProperty(const wchar_t* key, AMFData *obj);
	void AddProperty(const wchar_t* key, const AMFData &obj);
	AMFData& GetProperty(const wchar_t* key);
	bool HasProperty(const wchar_t* key) const;
	virtual AMFData* Clone() const;

private:
	// Prevent future problems with shallow copies for now by preventing them
	// Maybe later we will support deep copying of "elements"
	AMFEcmaArray(const AMFEcmaArray& rhs) = delete;
	AMFEcmaArray& operator=(const AMFEcmaArray& rhs) = delete;

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
	virtual bool IsParsed() const;
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType()  const {return TypedObject;};
	std::wstring GetClassName() const;

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
	virtual bool IsParsed() const;
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType() const {return StrictArray;};
	virtual void Dump() const;
	std::vector<AMFData*>& GetElements();
	DWORD GetLength() const;
	void AddElement(const wchar_t* value);
	void AddElement(const wchar_t* value, DWORD length);
	void AddElement(const std::wstring& value);
	void AddElement(const double value);
	void AddElement(bool value);
	void AddElement(AMFData* obj);
	void AddElement(AMFData& obj);
	virtual AMFData* Clone() const;
private:
	// Prevent future problems with shallow copies for now by preventing them
	// Maybe later we will support deep copying of "elements"
	AMFStrictArray(const AMFStrictArray& rhs) = delete;
	AMFStrictArray& operator=(const AMFStrictArray& rhs) = delete;

	std::vector<AMFData*> elements;
	AMFParser parser;
	U32Parser num;
};

class AMFDate : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed() const;
	//virtual DWORD Serialize(BYTE* data,DWORD size);		
	virtual ValueType GetType()  const {return Date;};
	time_t GetDate() const;
private:
	time_t value {};
};


class AMFMovieClip : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed()  const {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType()  const {return MovieClip;};
};

class AMFNull : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed()  const {return true;};
	virtual DWORD Serialize(BYTE* data,DWORD size);
	virtual ValueType GetType()  const {return Null;};
};

class AMFUndefined : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed()  const {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType()  const {return Undefined;};
};

class AMFReference : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size);
	virtual bool IsParsed() const;
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() const {return Reference;};
	WORD GetReference() const;
private:

	WORD value = 0;
};

class AMFUnsupported : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed()  const {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType()  const {return Unsupported;};
};

class AMFRecordSet : public AMFData
{
public:
	virtual DWORD Parse(BYTE *data,DWORD size) {return 0;};
	virtual bool IsParsed()  const {return true;};
	//virtual DWORD Serialize(BYTE* data,DWORD size) {return true;};		
	virtual ValueType GetType() const {return RecordSet;};
};

class AMFXmlDocument : public AMFString
{
public:
	ValueType GetType()  const {return XmlDocument;};
};


#endif
