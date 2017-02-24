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
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onMediaFrame(MediaFrame &frame) = 0;
		virtual void onMediaFrame(DWORD ssrc, MediaFrame &frame) = 0;
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

	static const char * TypeToString(Type type)
	{
		switch(type)
		{
			case Audio:
				return "Audio";
			case Video:
				return "Video";
			case Text:
				return "Text";
			default:
				return "Unknown";
		}
		return "Unknown";
	}

	MediaFrame(Type type,DWORD size)
	{
		//Set media type
		this->type = type;
		//Set no timestamp
		ts = (DWORD)-1;
		//No duration
		duration = 0;
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
	
	Type	GetType() const		{ return type;	}
	DWORD	GetTimeStamp() const	{ return ts;	}
	DWORD	SetTimestamp(DWORD ts)	{ this->ts = ts; }

	bool	HasRtpPacketizationInfo() const		{ return !rtpInfo.empty();	}
	const RtpPacketizationInfo& GetRtpPacketizationInfo() const { return rtpInfo;		}
	virtual MediaFrame* Clone() = 0;

	DWORD GetDuration() const		{ return duration;		}
	void SetDuration(DWORD duration)	{ this->duration = duration;	}

	BYTE* GetData() const			{ return buffer;		}
	DWORD GetLength() const			{ return length;		}
	DWORD GetMaxMediaLength() const		{ return bufferSize;		}

	void SetLength(DWORD length)		{ this->length = length;	}

	void Alloc(DWORD size)
	{
		//Calculate new size
		bufferSize = size;
		//Realloc
		buffer = (BYTE*) realloc(buffer,bufferSize);
	}

	void SetMedia(BYTE* data,DWORD size)
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

	DWORD AppendMedia(BYTE* data,DWORD size)
	{
		DWORD pos = length;
		//Check size
		if (size+length>bufferSize)
			//Allocate new size
			Alloc((size+length)*3/2);
		//Copy
		memcpy(buffer+length,data,size);
		//Increase length
		length+=size;
		//Return previous pos
		return pos;
	}
	
protected:
	Type type;
	DWORD ts;
	RtpPacketizationInfo rtpInfo;
	BYTE	*buffer;
	DWORD	length;
	DWORD	bufferSize;
	DWORD	duration;
	DWORD	clockRate;
};

#endif	/* MEDIA_H */

