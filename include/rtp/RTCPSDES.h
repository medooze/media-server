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
#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"
#include <vector>
#include <memory>

class RTCPSDES : public RTCPPacket
{
public:
	using shared = std::shared_ptr<RTCPSDES>;
public:
	class Item
	{
	public:
		using shared = std::shared_ptr<Item>;
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
		Item() = delete;
		Item(Item&&) = delete;
		Item(const Item&) = delete;
		Item(Type type,const BYTE* data,DWORD size)
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
// Ignore coverity error: Allocating insufficient memory for the terminating null of the string.
// coverity[alloc_strlen]
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
		using shared = std::shared_ptr<Description>;
	public:
		Description();
		Description(DWORD ssrc);
		~Description() = default;
		void Dump();
		DWORD GetSize();
		DWORD Parse(const BYTE* data,DWORD size);
		DWORD Serialize(BYTE* data,DWORD size);
		
		template<class ...Args>
		Item::shared CreateItem(Args... args)
		{
			auto desc =  std::make_shared<Item>(std::forward<Args>(args)...);
			AddItem(desc);
			return desc;
		}
		void		AddItem(const Item::shared& item)	{ items.push_back(item);	}
		DWORD		GetItemCount() const			{ return items.size();		}
		Item::shared	GetItem(BYTE i)				{ return items[i];		}

		DWORD		GetSSRC() const				{ return ssrc;			}
		void		SetSSRC(DWORD ssrc)			{ this->ssrc = ssrc;		}
	private:
		typedef std::vector<Item::shared> Items;
	private:
		DWORD ssrc = 0;
		Items items;
	};
public:
	RTCPSDES();
	virtual ~RTCPSDES() = default;
	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	template<class ...Args>
	Description::shared CreateDescription(Args... args)
	{
		auto desc =  std::make_shared<Description>(Description{ std::forward<Args>(args)... });
		AddDescription(desc);
		return desc;
	}
	
	void AddDescription(const Description::shared& desc)	{ descriptions.push_back(desc);	}
	DWORD GetDescriptionCount() const			{ return descriptions.size();	}
	Description::shared GetDescription(BYTE i)		{ return descriptions[i];	}
private:
	typedef std::vector<Description::shared> Descriptions;
private:
	Descriptions descriptions;
};

#endif /* RTCPSDES_H */

