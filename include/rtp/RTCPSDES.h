/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPSDES.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:02
 */

#ifndef RTCPSDES_H
#define RTCPSDES_H

class RTCPSDES : public RTCPPacket
{
public:

public:
	class Item
	{
	public:
		enum Type
		{
			CName = 1,
			Name = 2,
			Email = 3,
			Phone = 4,
			Location = 5,
			Tool = 6,
			Note = 7,
			Private = 8
		};
		static const char* TypeToString(Type type)
		{
			switch(type)
			{
				case CName:
					return "CName";
				case Name:
					return "Name";
				case Email:
					return "Email";
				case Phone:
					return "Phone";
				case Location:
					return "Location";
				case Tool:
					return "Tool";
				case Note:
					return "Note";
				case Private:
					return "Private";
			}
			return "Unknown";
		}
	public:
		Item(Type type,BYTE* data,DWORD size)
		{
			this->type = type;
			this->data = (BYTE*)malloc(size);
			this->size = size;
			memcpy(this->data,data,size);
		}
		Item(Type type,const char* str)
		{
			this->type = type;
			size = strlen(str);
			data = (BYTE*)malloc(size);
			memcpy(data,(BYTE*)str,size);
		}
		~Item()
		{
			free(data);
		}
		Type  GetType() const { return type; }
		BYTE* GetData() const { return data; }
		BYTE  GetSize() const { return size; }
	private:
		Type type;
		BYTE* data;
		BYTE size;
	};

	class Description
	{
	public:
		Description();
		Description(DWORD ssrc);
		~Description();
		void Dump();
		DWORD GetSize();
		DWORD Parse(BYTE* data,DWORD size);
		DWORD Serialize(BYTE* data,DWORD size);

		void AddItem(Item* item)	{ items.push_back(item);	}
		DWORD GetItemCount() const	{ return items.size();		}
		Item* GetItem(BYTE i)		{ return items[i];		}

		DWORD GetSSRC()	const		{ return ssrc;			}
		void  SetSSRC(DWORD ssrc)	{ this->ssrc = ssrc;		}
	private:
		typedef std::vector<Item*> Items;
	private:
		DWORD ssrc;
		Items items;
	};
public:
	RTCPSDES();
	~RTCPSDES();
	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	void AddDescription(Description* desc)	{ descriptions.push_back(desc);	}
	DWORD GetDescriptionCount() const	{ return descriptions.size();	}
	Description* GetDescription(BYTE i)	{ return descriptions[i];	}
private:
	typedef std::vector<Description*> Descriptions;
private:
	Descriptions descriptions;
};

#endif /* RTCPSDES_H */

