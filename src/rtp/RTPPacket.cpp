#include <array>

#include "codecs.h"
#include "rtp/RTPPacket.h"
#include "log.h"

RTPPayloadPool RTPPacket::PayloadPool(65536);

RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec, QWORD time) :
	//Create payload from pool
	payload(RTPPacket::PayloadPool.allocate())
{
	this->media = media;
	//Set codec
	this->codec = codec;
	//Default clock rates
	switch(media)
	{
		case MediaFrame::Video:
			clockRate = 90000;
			break;
		case MediaFrame::Audio:
			clockRate = AudioCodec::GetClockRate((AudioCodec::Type)codec);
			break;
		default:
			clockRate = 1000;
	}
	//We own the payload
	ownedPayload = true;
	//Set time
	this->time = time;
}

RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec) :
	RTPPacket(media, codec, ::getTimeMS())
{
}

RTPPacket::RTPPacket(MediaFrame::Type media, BYTE codec, const RTPHeader& header, const RTPHeaderExtension& extension) :
	RTPPacket(media, codec, header, extension, ::getTimeMS())
{
}

RTPPacket::RTPPacket(MediaFrame::Type media, BYTE codec, const RTPHeader& header, const RTPHeaderExtension& extension, QWORD time) :
	header(header),
	extension(extension),
	//Create payload from pool
	payload(RTPPacket::PayloadPool.allocate())
{
	this->media = media;
	//Set coced
	this->codec = codec;
	//Default clock rates
	switch(media)
	{
		case MediaFrame::Video:
			clockRate = 90000;
			break;
		case MediaFrame::Audio:
			clockRate = AudioCodec::GetClockRate((AudioCodec::Type)codec);
			break;
		default:
			clockRate = 1000;
	}
	//We own the payload
	ownedPayload = true;
	//Set time
	this->time = time;
}


RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec,const RTPHeader &header, const RTPHeaderExtension &extension, const RTPPayload::shared &payload, QWORD time) :
	header(header),
	extension(extension),
	payload(payload)
{
	this->media = media;
	//Set coced
	this->codec = codec;
	//Default clock rates
	switch(media)
	{
		case MediaFrame::Video:
			clockRate = 90000;
			break;
		case MediaFrame::Audio:
			clockRate = AudioCodec::GetClockRate((AudioCodec::Type)codec);
			break;
		default:
			clockRate = 1000;
	}
	//Copy payload object
	this->payload  = payload;
	//Owned payload
	ownedPayload = false;
	//Set time
	this->time = time;
}

RTPPacket::~RTPPacket()
{
}

RTPPacket::shared RTPPacket::Clone() const
{
	//New one
	auto cloned = std::make_shared<RTPPacket>(GetMediaType(),GetCodec(),GetRTPHeader(),GetRTPHeaderExtension(),payload,GetTime());
	//Set attrributes
	cloned->SetClockRate(GetClockRate());
	cloned->SetSeqCycles(GetSeqCycles());
	cloned->SetExtTimestamp(GetExtTimestamp());
	cloned->SetKeyFrame(IsKeyFrame());
	cloned->SetSenderTime(GetSenderTime());
	cloned->SetTimestampSkew(GetTimestampSkew());
	cloned->SetWidth(GetWidth());
	cloned->SetHeight(GetHeight());
	//Copy descriptors
	cloned->rewitePictureIds     = rewitePictureIds;
	cloned->vp8PayloadDescriptor = vp8PayloadDescriptor;
	cloned->vp8PayloadHeader     = vp8PayloadHeader;
	cloned->vp9PayloadDescriptor = vp9PayloadDescriptor;
	cloned->activeDecodeTargets  = activeDecodeTargets;
	cloned->templateDependencyStructure = templateDependencyStructure;
	cloned->config		     = config;
	//Return it
	return cloned;
}

RTPPacket::shared RTPPacket::Parse(const BYTE* data, DWORD size, const RTPMap& rtpMap, const RTPMap& extMap)
{
	return Parse(data,size,rtpMap,extMap,getTimeMS());
}

RTPPacket::shared RTPPacket::Parse(const BYTE* data, DWORD size, const RTPMap& rtpMap, const RTPMap& extMap, QWORD time)
{
	RTPHeader header;
	RTPHeaderExtension extension;
	//Parse RTP header
	DWORD ini = header.Parse(data,size);
	
	//On error
	if (!ini)
	{
		//Debug
		Debug("-RTPPacket::Parse() | Could not parse RTP header\n");
		//Dump it
		::Dump(data,size);
		//Exit
		return nullptr;
	}
	
	//If it has extension
	if (header.extension)
	{
		//Parse extension
		DWORD l = extension.Parse(extMap,data+ini,size-ini);
		//If not parsed
		if (!l)
		{
			///Debug
			Debug("-RTPPacket::Parse() | Could not parse RTP header extension\n");
			//Dump it
			::Dump(data,size);
			header.Dump();
			//Exit
			return nullptr;
		}
		//Inc ini
		ini += l;
	}

	//Check size with padding
	if (header.padding)
	{
		//Get last 2 bytes
		WORD padding = get1(data,size-1);
		//Ensure we have enought size
		if (size-ini<padding)
		{
			///Debug
			Debug("-RTPPacket::Parse() | RTP padding is bigger than size [padding:%u,size%u]\n",padding,size);
			//Exit
			return nullptr;
		}
		//Remove from size
		size -= padding;
	}
	
	//Get initial codec
	BYTE codec = rtpMap.GetCodecForType(header.payloadType);
	
	//Get media
	MediaFrame::Type media = GetMediaForCodec(codec);
	
	//Create normal packet
	auto packet = std::make_shared<RTPPacket>(media,codec,header,extension,time);
	
	//Set the payload
	packet->SetPayload(data+ini,size-ini);
	
	//Done
	return packet;
}


DWORD RTPPacket::Serialize(BYTE* data,DWORD size,const RTPMap& extMap) const
{
	//Serialize header
	uint32_t len = header.Serialize(data,size);

	//Check
	if (!len)
		//Error
		return Error("-RTPPacket::Serialize() | Error serializing rtp headers\n");

	//If we have extension
	if (header.extension)
	{
		//Serialize
		uint32_t n = extension.Serialize(extMap,data+len,size-len);
		//Comprobamos que quepan
		if (!n)
			//Error
			return Error("-RTPPacket::Serialize() | Error serializing rtp extension headers\n");
		//Inc len
		len += n;
	}

	//Ensure we have enougth data
	if (len+GetMediaLength()>size)
		//Error
		return Error("-RTPPacket::Serialize() | Media overflow\n");
	
	//If we have osn
	if (osn)
	{
		//And set the original seq
		set2(data, len, *osn);
		//Move payload start
		len += 2;
	}
	
	//If we need to rewrite the vp8 descriptor
	if (rewitePictureIds && vp8PayloadDescriptor)
	{
		//Get current vp8 descriptor length
		uint32_t descLen = vp8PayloadDescriptor->GetSize();
		//Copy the descriptor
		auto vp8NewPayloadDescriptor = vp8PayloadDescriptor.value();
		//Always store it as two bytes
		vp8NewPayloadDescriptor.pictureIdPresent = 1;
		vp8NewPayloadDescriptor.pictureIdLength = 2;
		//Write it back
		len += vp8NewPayloadDescriptor.Serialize(data+len,size-len);
		
		//Check size
		if (descLen>GetMediaLength())
			//Error
			return Error("-RTPPacket::Serialize() | Wrong vp8PayloadDescriptor when rewriting pict ids\n");
		
		//Copy media payload without the old description
		memcpy(data+len,GetMediaData()+descLen,GetMediaLength()-descLen);
		//Inc payload len
		len += GetMediaLength()-descLen;
		
	} else {
		//Copy media payload
		memcpy(data+len,GetMediaData(),GetMediaLength());
		//Inc payload len
		len += GetMediaLength();
	}
	
	
	//Return copied len
	return len;
}

BYTE* RTPPacket::AdquireMediaData()
{
	//If the packet was cloned and doesn't own the payload
	if (!ownedPayload)
	{
		//Store old one
		RTPPayload::shared old = payload;
		//Create payload from pool
		payload = RTPPacket::PayloadPool.allocate();
		//Clone payload
		payload->SetPayload(*old);
		//We own the payload
		ownedPayload = true;
	}
	//You can write on payload now
	return payload->GetMediaData();
}

bool RTPPacket::RecoverOSN()
{
	/*
	The format of a retransmission packet is shown below:
	 0                   1                   2                   3
	 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                         RTP Header                            |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|            OSN                |                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
	|                  Original RTP Packet Payload                  |
	|                                                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	//Ensure we have enought data
	if (GetMediaLength()<2)
		//error
	       return false;
	
	//Get original sequence number
	WORD osn = get2(GetMediaData(),0);
	
	 //Skip osn from payload
	if (!SkipPayload(2))
		//error
		return false;

	//Set original seq num
	SetSeqNum(osn);
	
	//Done
	return true;
}

void RTPPacket::SetOSN(DWORD extSeqNum)
{
	//Store current seq as osn
	osn = GetSeqNum();
	//Set new seq num
	SetExtSeqNum(extSeqNum);
}

void RTPPacket::Dump() const
{
	Debug("[RTPPacket %s codec=%s payload=%d extSeqNum=%u(%u) senderTime=%llu]\n",MediaFrame::TypeToString(GetMediaType()),GetNameForCodec(GetMediaType(),GetCodec()),GetMediaLength(),GetExtSeqNum(),GetSeqCycles(),senderTime);
	header.Dump();
	//If  there is an extension
	if (header.extension)
		//Dump extension
		extension.Dump();
	if (vp8PayloadDescriptor)
		vp8PayloadDescriptor->Dump();
	if (vp8PayloadHeader)
		vp8PayloadHeader->Dump();
	::Dump(GetMediaData(),16);
	Debug("[/RTPPacket]\n");
}


bool RTPPacket::ParseDependencyDescriptor(const std::optional<TemplateDependencyStructure>& templateDependencyStructure, std::optional<std::vector<bool>>& activeDecodeTargets)
{
	//parse it
	if (!extension.ParseDependencyDescriptor(templateDependencyStructure))
		//Nothing to do
		return false;

	//If packet has a new dependency structure
	if (extension.dependencyDescryptor && extension.dependencyDescryptor->templateDependencyStructure)
	{
		//Store it
		this->templateDependencyStructure = extension.dependencyDescryptor->templateDependencyStructure;
		this->activeDecodeTargets	  = extension.dependencyDescryptor->activeDecodeTargets;
	} else {
		//Keep previous
		this->templateDependencyStructure = templateDependencyStructure;
		this->activeDecodeTargets	  = extension.dependencyDescryptor && extension.dependencyDescryptor->activeDecodeTargets.has_value() ?
			extension.dependencyDescryptor->activeDecodeTargets : activeDecodeTargets;
	}
	//Done
	return true;
}
