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
	DWORD Parse(const BYTE *data,DWORD size);
	bool IsParsed() const;
	WORD GetValue() const;
	void SetValue(WORD value);
	DWORD Serialize(BYTE *data,DWORD size) const;
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
	DWORD Parse(const BYTE *data,DWORD size);
	bool IsParsed() const;
	DWORD GetValue() const;
	void SetValue(DWORD value);
	DWORD Serialize(BYTE *data,DWORD size) const;
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
	DWORD Parse(const BYTE *data,DWORD size);
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
	virtual DWORD Parse(const BYTE* buffer,DWORD size) = 0;
	virtual bool IsParsed() const = 0 ; 
	virtual DWORD Serialize(BYTE* buffer,DWORD size) const;
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
	DWORD Parse(const BYTE *data,DWORD size);
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
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	virtual DWORD GetSize() const override;
	virtual DWORD Serialize(BYTE* data,DWORD size) const override;
	virtual ValueType GetType()  const  override {return Number;};
	double GetNumber() const;
	void SetNumber(double value);
	virtual void Dump() const override;
	virtual AMFData* Clone() const override;
private:
	QWORD value;
};

class AMFBoolean : public AMFData
{
public:
	AMFBoolean();
	AMFBoolean(bool val);
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	virtual DWORD Serialize(BYTE* data,DWORD size) const override;
	virtual DWORD GetSize() const override;
	virtual ValueType GetType() const  override {return Boolean;};
	bool GetBoolean() const;
	void SetBoolean(bool value);
	virtual AMFData* Clone() const override;
	virtual void Dump() const override;
private:
	bool value;
};

class AMFString : public AMFData
{
public:
	AMFString();
	AMFString(const wchar_t *val);
	AMFString(const std::wstring& val);
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	virtual DWORD GetSize() const override;
	virtual DWORD Serialize(BYTE* data,DWORD size) const override;
	virtual ValueType GetType()  const  override {return String;};
	virtual void Reset() override;
	std::wstring GetWString() const;
	const wchar_t* GetWChar() const;
	void SetWString(const std::wstring& value);
	DWORD GetUTF8Size() const;
	std::string GetUTF8String() const;
	virtual void Dump() const override;
	virtual AMFData* Clone() const override;
private:
	UTF8Parser utf8parser;
	U16Parser u16parser;
};

class AMFLongString : public AMFData
{
public:
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;		
	virtual ValueType GetType() const  override {return LongString;};
	virtual void Reset() override;
	std::wstring GetWString() const;
	std::string GetUTF8String() const;
	DWORD GetUTF8Size() const;
private:
	UTF8Parser utf8parser;
	U32Parser u32parser;
};

typedef std::map<std::wstring,AMFData*> AMFObjectMap;

class AMFNamedPropertiesObject
{
public:
	virtual AMFObjectMap& GetProperties() = 0;
	virtual void AddProperty(const wchar_t* key, const wchar_t* value) = 0;
	virtual void AddProperty(const wchar_t* key, const std::wstring& value) = 0;
	virtual void AddProperty(const wchar_t* key, const wchar_t* value, DWORD length) = 0;
	virtual void AddProperty(const wchar_t* key, const double value) = 0;
	virtual void AddProperty(const wchar_t* key, const bool value) = 0;
	virtual void AddProperty(const wchar_t* key, AMFData* obj) = 0;
	virtual void AddProperty(const wchar_t* key, const AMFData& obj) = 0;
	virtual AMFData& GetProperty(const wchar_t* key) = 0;
	virtual bool HasProperty(const wchar_t* key) const = 0;

};

class AMFObject : public AMFData, AMFNamedPropertiesObject
{
public:
	AMFObject();
	~AMFObject();
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual DWORD GetSize() const override;
	virtual DWORD Serialize(BYTE* buffer,DWORD size) const override;
	virtual bool IsParsed() const override;
	virtual ValueType GetType()  const  override {return Object;};

	virtual AMFObjectMap& GetProperties() override;
	virtual void AddProperty(const wchar_t* key, const wchar_t* value) override;
	virtual void AddProperty(const wchar_t* key, const std::wstring& value) override;
	virtual void AddProperty(const wchar_t* key, const wchar_t* value, DWORD length) override;
	virtual void AddProperty(const wchar_t* key, const double value) override;
	virtual void AddProperty(const wchar_t* key, const bool value) override;
	virtual void AddProperty(const wchar_t* key, AMFData *obj) override;
	virtual void AddProperty(const wchar_t* key, const AMFData &obj) override;
	virtual AMFData& GetProperty(const wchar_t* key) override;
	virtual bool HasProperty(const wchar_t* key) const override;

	virtual void Dump() const override;
	virtual AMFData* Clone() const override;

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

class AMFEcmaArray : public AMFData, AMFNamedPropertiesObject
{
public:
	AMFEcmaArray();
	~AMFEcmaArray();
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	virtual DWORD Serialize(BYTE* data,DWORD size) const override;
	virtual DWORD GetSize() const override;
	virtual ValueType GetType()  const  override {return EcmaArray;};
	virtual void Dump() const override;

	virtual AMFObjectMap& GetProperties() override;
	virtual void AddProperty(const wchar_t* key, const wchar_t* value) override;
	virtual void AddProperty(const wchar_t* key, const std::wstring& value) override;
	virtual void AddProperty(const wchar_t* key, const wchar_t* value, DWORD length) override;
	virtual void AddProperty(const wchar_t* key, const double value) override;
	virtual void AddProperty(const wchar_t* key, const bool value) override;
	virtual void AddProperty(const wchar_t* key, AMFData* obj) override;
	virtual void AddProperty(const wchar_t* key, const AMFData& obj) override;
	virtual AMFData& GetProperty(const wchar_t* key) override;
	virtual bool HasProperty(const wchar_t* key) const override;
	
	virtual AMFData* Clone() const override;

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
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;		
	virtual ValueType GetType()  const  override {return TypedObject;};
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
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;		
	virtual ValueType GetType() const override {return StrictArray;};
	virtual void Dump() const override;
	std::vector<AMFData*>& GetElements();
	DWORD GetLength() const;
	void AddElement(const wchar_t* value);
	void AddElement(const wchar_t* value, DWORD length);
	void AddElement(const std::wstring& value);
	void AddElement(const double value);
	void AddElement(bool value);
	void AddElement(AMFData* obj);
	void AddElement(AMFData& obj);
	virtual AMFData* Clone() const override;
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
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;		
	virtual ValueType GetType() const override {return Date;};
	time_t GetDate() const;
private:
	time_t value {};
};


class AMFMovieClip : public AMFData
{
public:
	virtual DWORD Parse(const BYTE *data,DWORD size) override {return 0;};
	virtual bool IsParsed()  const override {return true;};
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;		
	virtual ValueType GetType()  const  override {return MovieClip;};
};

class AMFNull : public AMFData
{
public:
	virtual DWORD Parse(const BYTE *data,DWORD size)  override {return 0;};
	virtual bool IsParsed()  const  override {return true;};
	virtual DWORD Serialize(BYTE* data,DWORD size) const override;
	virtual ValueType GetType()  const  override {return Null;};
};

class AMFUndefined : public AMFData
{
public:
	virtual DWORD Parse(const BYTE *data,DWORD size)  override {return 0;};
	virtual bool IsParsed()  const  override {return true;};
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;		
	virtual ValueType GetType()  const override {return Undefined;};
};

class AMFReference : public AMFData
{
public:
	virtual DWORD Parse(const BYTE *data,DWORD size) override;
	virtual bool IsParsed() const override;
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;	
	virtual ValueType GetType() const  override {return Reference;};
	WORD GetReference() const;
private:

	WORD value = 0;
};

class AMFUnsupported : public AMFData
{
public:
	virtual DWORD Parse(const BYTE *data,DWORD size)  override {return 0;};
	virtual bool IsParsed()  const  override {return true;};
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;	
	virtual ValueType GetType()  const  override {return Unsupported;};
};

class AMFRecordSet : public AMFData
{
public:
	virtual DWORD Parse(const BYTE *data,DWORD size)  override {return 0;};
	virtual bool IsParsed()  const  override {return true;};
	//TODO: Implement
	//virtual DWORD Serialize(BYTE* data,DWORD size) const;	
	virtual ValueType GetType() const  override {return RecordSet;};
};

class AMFXmlDocument : public AMFString
{
public:
	ValueType GetType()  const {return XmlDocument;};
};


#endif
