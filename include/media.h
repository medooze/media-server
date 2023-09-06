#ifndef _MEDIA_H_
#define	_MEDIA_H_
#include "config.h"
#include "log.h"
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <memory>
#include "Buffer.h"
#include "BufferReader.h"

class MediaFrame
{
public:
	class Listener
	{
	public:
		using shared = std::shared_ptr<Listener>;
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onMediaFrame(const MediaFrame &frame) = 0;
		virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame) = 0;
	};

	class Producer
	{
	public:
		using shared = std::shared_ptr<Producer>;
	public:
		//Virtual desctructor
		virtual ~Producer() {};
	public:
		//Interface
		virtual void AddMediaListener(const MediaFrame::Listener::shared& listener) = 0;
		virtual void RemoveMediaListener(const MediaFrame::Listener::shared& listener) = 0;
	};

	class RtpPacketization
	{
	public:
		RtpPacketization(DWORD pos,DWORD size,const BYTE* prefix,DWORD prefixLen)
		{
			//Store values
			this->pos = pos;
			this->size = size;
			this->prefixLen = prefixLen;
			//Check size
			if (prefixLen)
			{
				//Allocate mem
				this->prefix = (BYTE*) malloc(prefixLen);
				//Copy data
				memcpy(this->prefix,prefix,prefixLen);
			}
		}
		RtpPacketization(RtpPacketization&& info)
		{
			//Store values
			this->pos = info.GetPos();
			this->size = info.GetSize();
			this->prefixLen = info.GetPrefixLen();
			//Check size
			if (prefixLen)
			{
				//Allocate mem
				this->prefix = (BYTE*) malloc(prefixLen);
				//Copy data
				memcpy(this->prefix,info.GetPrefixData(),prefixLen);
			}
		}
		~RtpPacketization()
		{
			if (prefix) free(prefix);
		}
		void  SetPos(DWORD pos)	      { this->pos = pos;	}
		void  IncPos(DWORD num)	      { this->pos += num;	}		
		DWORD GetPos()		const { return pos;		}
		DWORD GetSize()		const { return size;		}
		BYTE* GetPrefixData()	const { return prefix;		}
		DWORD GetPrefixLen()	const { return prefixLen;	}
		DWORD GetTotalLength()	const { return size+prefixLen;	}
		
		void Dump() const 
		{
			if (!prefixLen)
			{
				Debug("[RtpPacketization size=%u pos=%u/]\n",size,pos);
			} else {
				Debug("[RtpPacketization size=%u pos=%u]\n",size,pos);
				::Dump(prefix,prefixLen);
				Debug("[RtpPacketization/]\n");
			}
			
		}
	private:
		DWORD	pos		= 0;
		DWORD	size		= 0;
		BYTE*	prefix		= nullptr;
		DWORD	prefixLen	= 0;
	};

	typedef std::vector<RtpPacketization> RtpPacketizationInfo;
public:
	enum Type {Audio=0,Video=1,Text=2,Unknown=-1};

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
	}

	MediaFrame(Type type)
	{
		//Set media type
		this->type = type;
		//Create new owned buffer
		buffer = std::make_shared<Buffer>();
		//Owned buffer
		ownedBuffer = true;
	}

	MediaFrame(Type type,DWORD size)
	{
		//Set media type
		this->type = type;
		//Create new owned buffer
		buffer = std::make_shared<Buffer>(size);
		//Owned buffer
		ownedBuffer = true;
	}
	
	MediaFrame(Type type,const std::shared_ptr<Buffer>& buffer)
	{
		//Set media type
		this->type = type;
		//Share same buffer
		this->buffer = buffer;
		//Not owned buffer
		ownedBuffer = false;
	}

	MediaFrame(Type type, Buffer&& buffer)
	{
		//Set media type
		this->type = type;
		//Create new buffer
		this->buffer = std::make_shared<Buffer>(std::move(buffer));
		//owned buffer
		ownedBuffer = true;
	}

	virtual ~MediaFrame()
	{
		//Clear
		ClearRTPPacketizationInfo();
	}

	void	ClearRTPPacketizationInfo()
	{
		//Clear
		rtpInfo.clear();
	}
	
	void	DumpRTPPacketizationInfo() const
	{
		//Dump all info
		for (const auto& info : rtpInfo)
			info.Dump();
	}
	
	void	AddRtpPacket(DWORD pos,DWORD size,const BYTE* prefix = nullptr,DWORD prefixLen = 0)		
	{
		rtpInfo.emplace_back(pos,size,prefix,prefixLen);
	}
	
	Type	GetType() const		{ return type;		}
	QWORD	GetTimeStamp() const	{ return ts;		} //Deprecated
	QWORD	GetTimestamp() const	{ return ts;		}
	void	SetTimestamp(QWORD ts)	{ this->ts = ts;	}
	QWORD	GetTime() const		{ return time;		}
	void	SetTime(QWORD time)	{ this->time = time;	}
	
	DWORD	GetSSRC() const		{ return ssrc;		}
	void	SetSSRC(DWORD ssrc)	{ this->ssrc = ssrc;	}
	
	QWORD GetSenderTime() const		{ return senderTime;			}
	void  SetSenderTime(QWORD senderTime )	{ this->senderTime = senderTime;	}
			
	int64_t GetTimestampSkew() const { return timestampSkew; } 
	void  SetTimestampSkew(int64_t timestampSkew) { this->timestampSkew = timestampSkew; } 
		
	bool	HasRtpPacketizationInfo() const				{ return !rtpInfo.empty();	}
	const RtpPacketizationInfo& GetRtpPacketizationInfo() const	{ return rtpInfo;		}
	virtual MediaFrame* Clone() const = 0;

	DWORD GetDuration() const		{ return duration;		}
	void SetDuration(DWORD duration)	{ this->duration = duration;	}

	DWORD GetLength() const			{ return buffer->GetSize();			}
	DWORD GetMaxMediaLength() const		{ return buffer->GetCapacity();			}

#ifndef SWIGGO
	// the SWIG compiler can not handle correctly the 2 GetData signatures for the GoLang target
	const BYTE* GetData() const		{ return buffer->GetData();			}
#endif
	BYTE* GetData()				{ AdquireBuffer(); return buffer->GetData();	}
	const Buffer::shared& GetBuffer() const	{ return buffer; }
	void SetLength(DWORD length)		{ AdquireBuffer(); buffer->SetSize(length);	}
	
	void DisableSharedBuffer()		{ disableSharedBuffer = true;			}
	
	void ResetData(DWORD size = 0) 
	{
		//Create new owned buffer
		buffer = std::make_shared<Buffer>(size);
		//Owned buffer
		ownedBuffer = true;
	}

	void Alloc(DWORD size)
	{
		//Adquire buffer
		AdquireBuffer();
		//Allocate mem
		buffer->Alloc(size);
	}

	void SetMedia(const BYTE* data,DWORD size)
	{
		//Adquire buffer
		AdquireBuffer();
		//Allocate mem
		buffer->SetData(data,size);
	}

	DWORD AppendMedia(const BYTE* data,DWORD size)
	{
                //Get current pos
                DWORD pos = buffer->GetSize();
		//Adquire buffer
		AdquireBuffer();
		//Append data
		buffer->AppendData(data,size);
		//Return previous pos
		return pos;
	}

	DWORD AppendMedia(BufferReader& reader, DWORD size)
	{
		//Get current pos
		DWORD pos = buffer->GetSize();
		//Adquire buffer
		AdquireBuffer();
		//Append data
		buffer->AppendData(reader.GetData(size), size);
		//Return previous pos
		return pos;
	}

	DWORD AppendMedia(const Buffer& append)
	{
		//Get current pos
		DWORD pos = buffer->GetSize();
		//Adquire buffer
		AdquireBuffer();
		//Append data
		buffer->AppendData(append.GetData(), append.GetSize());
		//Return previous pos
		return pos;
	}

	DWORD AppendMedia(BufferReader& reader)
	{
		return AppendMedia(reader, reader.GetLeft());
	}

	void PrependMedia(const BYTE* data,DWORD size)
	{
		//Store old buffer
		auto old = buffer;
		//New one
		buffer = std::make_shared<Buffer>(old->GetSize()+size);
		//We own the payload
		ownedBuffer = true;
		//Append data
		buffer->AppendData(data,size);
		//Append data
		buffer->AppendData(old->GetData(),old->GetSize());
		//Move rtp packets
		for (auto &info : rtpInfo)
			//Move init of rtp packet
			info.IncPos(size);
	}

	void PrependMedia(const Buffer& buffer)
	{
		PrependMedia(buffer.GetData(),buffer.GetSize());
	}
        
	BYTE* AllocateCodecConfig(DWORD size)
        {
		//Create new config buffer
		config = std::make_shared<Buffer>(size);
		//Set new size
		config->SetSize(size);
                //return it
                return config->GetData();
        }
        
	void SetCodecConfig(const BYTE* data,DWORD size)
	{
		//Allocate new config;
		AllocateCodecConfig(size);
		//Set data
		config->SetData(data,size);
	}
	
	void ClearCodecConfig()
	{
		//Free mem
		config.reset();
	}
	
	void Reset() 
	{
		//Clear packetization info
		ClearRTPPacketizationInfo();
		//Reset data
		ResetData();
		//Clear config
		ClearCodecConfig();
		//Clear time
		SetTimestamp((DWORD)-1);
		SetTime(0);
		SetDuration(0);
	}
	
	bool HasCodecConfig() const			{ return config && !config->IsEmpty();		}
	BYTE* GetCodecConfigData() const		{ return config ? config->GetData() : nullptr;	}
	DWORD GetCodecConfigSize() const		{ return config ? config->GetSize() : 0;	}
	const Buffer::shared& GetCodecConfig() const	{ return config;				}
	DWORD GetClockRate() const			{ return clockRate;				}
	void  SetClockRate(DWORD clockRate)		{ this->clockRate = clockRate;			}

protected:
	void AdquireBuffer()
	{
		//If already owning
		if (ownedBuffer)
			//Do nothing
			return;
		
		//Clone payload
		buffer = std::make_shared<Buffer>(buffer->GetData(),buffer->GetSize());
		//We own the payload
		ownedBuffer = true;
	}
	
protected:
	Type type			= MediaFrame::Unknown;
	QWORD ts			= (QWORD)-1;
	QWORD time			= 0;
	QWORD senderTime		= 0;
	DWORD ssrc			= 0;
	int64_t timestampSkew 	= 0;
	
	Buffer::shared	buffer;
	bool ownedBuffer		= false;
	bool disableSharedBuffer	= false;
	
	DWORD	duration		= 0;
	DWORD	clockRate		= 1000;
	
	RtpPacketizationInfo rtpInfo;
	Buffer::shared config;
};

#endif	/* MEDIA_H */

