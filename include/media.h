#ifndef _MEDIA_H_
#define	_MEDIA_H_
#include "config.h"
#include <stdlib.h>
#include <vector>
#include <string.h>

class MediaFrame
{
public:
	class Listener
	{
	public:
		virtual void onMediaFrame(MediaFrame &frame) = 0;
	};

	class RtpPacketization
	{
	public:
		RtpPacketization(DWORD pos,DWORD size,BYTE* prefix,DWORD prefixLen)
		{
			//Store values
			this->pos = pos;
			this->size = size;
			this->prefixLen = prefixLen;
			//Check size
			if (prefixLen)
				//Copy
				memcpy(this->prefix,prefix,prefixLen);

		}

		DWORD GetPos()		{ return pos;	}
		DWORD GetSize()		{ return size;	}
		BYTE* GetPrefixData()	{ return prefix;	}
		DWORD GetPrefixLen()	{ return prefixLen;	}
		DWORD GetTotalLength()	{ return size+prefixLen;}
		
	private:
		DWORD	pos;
		DWORD	size;
		BYTE	prefix[16];
		DWORD	prefixLen;
	};

	typedef std::vector<RtpPacketization*> RtpPacketizationInfo;
public:
	enum Type {Audio=0,Video=1,Text=2};

	MediaFrame(Type type,DWORD size)
	{
		//Set media type
		this->type = type;
		//Set no timestamp
		ts = (DWORD)-1;
		//Set buffer size
		bufferSize = size;
		//Allocate memory
		buffer = (BYTE*) malloc(bufferSize);
		//NO length
		length = 0;
	}

	virtual ~MediaFrame()
	{
		//Clear
		ClearRTPPacketizationInfo();
		//Clear memory
		free(buffer);
	}

	void	ClearRTPPacketizationInfo()
	{
		//Emtpy
		while (!rtpInfo.empty())
		{
			//Delete
			delete(rtpInfo.back());
			//remove
			rtpInfo.pop_back();
		}
	}
	
	void	AddRtpPacket(DWORD pos,DWORD size,BYTE* prefix,DWORD prefixLen)		
	{
		rtpInfo.push_back(new RtpPacketization(pos,size,prefix,prefixLen));
	}
	
	Type	GetType()		{ return type;	}
	DWORD	GetTimeStamp()		{ return ts;	}
	DWORD	SetTimestamp(DWORD ts)	{ this->ts = ts; }
	bool	HasRtpPacketizationInfo()		{ return !rtpInfo.empty();	}
	RtpPacketizationInfo& GetRtpPacketizationInfo()	{ return rtpInfo;		}
	virtual MediaFrame* Clone() = 0;

	BYTE* GetData()			{ return buffer;		}
	DWORD GetLength()		{ return length;		}
	DWORD GetMaxMediaLength()	{ return bufferSize;		}

	void SetLength(DWORD length)	{ this->length = length;	}

	bool Alloc(DWORD size)
	{
		//Calculate new size
		bufferSize = size;
		//Realloc
		buffer = (BYTE*) realloc(buffer,bufferSize);
	}

	bool SetMedia(BYTE* data,DWORD size)
	{
		//Check size
		if (size>bufferSize)
			//Allocate new size
			Alloc(size*3/2);
		//Copy
		memcpy(buffer,data,size);
		//Increase length
		length=size;
	}

	bool AppendMedia(BYTE* data,DWORD size)
	{
		//Check size
		if (size+length>bufferSize)
			//Allocate new size
			Alloc((size+length)*3/2);
		//Copy
		memcpy(buffer+length,data,size);
		//Increase length
		length+=size;
	}
	
protected:
	Type type;
	DWORD ts;
	RtpPacketizationInfo rtpInfo;
	BYTE	*buffer;
	DWORD	length;
	DWORD	bufferSize;
};

#endif	/* MEDIA_H */

