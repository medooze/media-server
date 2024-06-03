#ifndef RTPPACKET_H
#define RTPPACKET_H
#include "config.h"
#include "media.h"
#include "rtp/RTPHeader.h"
#include "rtp/RTPHeaderExtension.h"
#include "rtp/RTPPayload.h"
#include "rtp/RTPPayloadPool.h"
#include "h264/h264.h"
#include "vp8/vp8.h"
#include "vp9/VP9PayloadDescription.h"
#include <memory>
#include <optional>

class RTPPacket
{
public:
	const static DWORD MaxExtSeqNum = 0xFFFFFFFF;
	static RTPPayloadPool PayloadPool;
public:
	using shared = std::shared_ptr<RTPPacket>;
	using unique = std::unique_ptr<RTPPacket>;
	
public:
	static RTPPacket::shared Parse(const BYTE* data, DWORD size, const RTPMap& rtpMap, const RTPMap& extMap);
	static RTPPacket::shared Parse(const BYTE* data, DWORD size, const RTPMap& rtpMap, const RTPMap& extMap, QWORD time);
public:
	RTPPacket(MediaFrame::Type media, BYTE codec);
	RTPPacket(MediaFrame::Type media, BYTE codec, QWORD time);
	RTPPacket(MediaFrame::Type media, BYTE codec, const RTPHeader& header, const RTPHeaderExtension& extension);
	RTPPacket(MediaFrame::Type media, BYTE codec, const RTPHeader& header, const RTPHeaderExtension& extension, QWORD time);
	RTPPacket(MediaFrame::Type media, BYTE codec, const RTPHeader& header, const RTPHeaderExtension& extension, const RTPPayload::shared& payload, QWORD time);
	virtual ~RTPPacket();

	RTPPacket::shared Clone() const;
	
	DWORD Serialize(BYTE* data,DWORD size,const RTPMap& extMap) const;
	
	bool SetPayload(const BYTE *data,DWORD size)	{ return payload->SetPayload(data,size);	}
	bool SkipPayload(DWORD skip)			{ return payload->SkipPayload(skip);		}
	bool PrefixPayload(BYTE *data,DWORD size)	{ return payload->PrefixPayload(data,size);	}
	
	bool RecoverOSN();
	void SetOSN(DWORD extSeqNum);

	virtual void Dump() const;
	
	//Setters
	void SetTimestamp(DWORD timestamp)	{ header.timestamp = timestamp;		}
	void SetExtTimestamp(QWORD extTimestamp){ header.timestamp = (DWORD)extTimestamp; this->timestampCycles = extTimestamp >> 32;	}
	void SetSeqNum(WORD seq)		{ header.sequenceNumber = seq;		}
	void SetExtSeqNum(DWORD extSeq)		{ header.sequenceNumber = (WORD)extSeq; this->seqCycles = extSeq >> 16;			}
	void SetMark(bool mark)			{ header.mark = mark;			}
	void SetSSRC(DWORD ssrc)		{ header.ssrc = ssrc;			}
	void SetPayloadType(DWORD payloadType)	{ header.payloadType = payloadType;	}
	void SetType(DWORD payloadType)		{ SetPayloadType(payloadType);		} //Deprecated
	void SetPadding(WORD padding)		{ header.padding = padding;		}
	void SetMediaTpe(MediaFrame::Type media){ this->media = media;			}
	void SetCodec(BYTE codec)		{ this->codec = codec;			}
	void SetSeqCycles(WORD cycles)		{ this->seqCycles = cycles;		}
	void SetTimestampCycles(DWORD cycles)	{ this->timestampCycles = cycles;	}
	void SetClockRate(DWORD rate)		{ this->clockRate = rate;		}

	void SetMediaLength(DWORD len)		{ payload->SetMediaLength(len);		}
	
	//Getters
	MediaFrame::Type GetMedia()	const { return media;				} //Deprecated
	MediaFrame::Type GetMediaType()	const { return media;				}
	BYTE  GetCodec()		const { return codec;				}
	
	BYTE* AdquireMediaData();
	const BYTE* GetMediaData()	const { return payload ? payload->GetMediaData()	: nullptr;	}
	DWORD GetMediaLength()		const { return payload ? payload->GetMediaLength()	: 0; 		}
	DWORD GetMaxMediaLength()	const { return payload ? payload->GetMaxMediaLength()	: 0;		}
	
	bool  GetMark()			const { return header.mark;			}
	DWORD GetTimestamp()		const { return header.timestamp;		}
	WORD  GetSeqNum()		const { return header.sequenceNumber;		}
	DWORD GetSSRC()			const { return header.ssrc;			}
	DWORD GetPayloadType()		const { return header.payloadType;		}
	WORD  GetPadding()		const { return header.padding;			}
	
	WORD  GetSeqCycles()		const { return seqCycles;			}
	DWORD GetClockRate()		const { return clockRate;			}
	DWORD GetExtSeqNum()		const { return ((DWORD)seqCycles)<<16 | GetSeqNum();			}
	QWORD GetExtTimestamp()		const { return ((QWORD)timestampCycles)<<32 | GetTimestamp();		}
	QWORD GetClockTimestamp()	const { return static_cast<QWORD>(GetExtTimestamp())*1000/clockRate;	}

	//Extensions
	void  SetAbsSentTime(QWORD absSentTime)						{ header.extension = extension.hasAbsSentTime		= true; extension.absSentTime = absSentTime;	}
	void  SetTimeOffset(int timeOffset)						{ header.extension = extension.hasTimeOffset		= true; extension.timeOffset = timeOffset;	}
	void  SetTransportSeqNum(DWORD seq)						{ header.extension = extension.hasTransportWideCC	= true; extension.transportSeqNum = seq;	}
	void  SetFrameMarkings(const RTPHeaderExtension::FrameMarks& frameMarks )	{ header.extension = extension.hasFrameMarking		= true; extension.frameMarks = frameMarks;	}
	void  SetRId(const std::string &rid)						{ header.extension = extension.hasRId			= true; extension.rid = rid;			}
	void  SetRepairedId(const std::string &repairedId)				{ header.extension = extension.hasRepairedId		= true; extension.repairedId = repairedId;	}
	void  SetMediaStreamId(const std::string &mid)					{ header.extension = extension.hasMediaStreamId		= true; extension.mid = mid;			}
	void  SetDependencyDescriptor(DependencyDescriptor& dependencyDescriptor)	{ header.extension = extension.hasDependencyDescriptor	= true; extension.dependencyDescryptor = dependencyDescriptor;		}
	void  SetAbsoluteCaptureTimestamp(QWORD ntp)					{ header.extension = extension.hasAbsoluteCaptureTime	= true; extension.absoluteCaptureTime.SetAbsoluteCaptureTimestamp(ntp); }
	void  SetAbsoluteCaptureTime(QWORD ms)						{ header.extension = extension.hasAbsoluteCaptureTime	= true; extension.absoluteCaptureTime.SetAbsoluteCaptureTime(ms);	}
	void  SetPlayoutDelay(uint16_t min, uint16_t max)				{ header.extension = extension.hasPlayoutDelay		= true; extension.playoutDelay.SetPlayoutDelay(min, max);		}
	void  SetPlayoutDelay(const struct RTPHeaderExtension::PlayoutDelay& playoutDelay)	{ header.extension = extension.hasPlayoutDelay		= true; extension.playoutDelay = playoutDelay;				}
	void  SetColorSpace(const struct RTPHeaderExtension::ColorSpace& colorSpace)		{ header.extension = extension.hasColorSpace		= true; extension.colorSpace = colorSpace;				}
	void  SetVideoLayersAllocation(const VideoLayersAllocation& videoLayersAllocation)	{ header.extension = extension.hasVideoLayersAllocation = true; extension.videoLayersAllocation = videoLayersAllocation;	}
	
	bool  ParseDependencyDescriptor(const std::optional<TemplateDependencyStructure>& templateDependencyStructure, std::optional<std::vector<bool>>& activeDecodeTargets);
	
	//Disable extensions
	void  DisableAbsSentTime()		{ extension.hasAbsSentTime		= false; CheckExtensionMark(); }
	void  DisableTimeOffset()		{ extension.hasTimeOffset		= false; CheckExtensionMark(); }
	void  DisableTransportSeqNum()		{ extension.hasTransportWideCC		= false; CheckExtensionMark(); }
	void  DisableFrameMarkings()		{ extension.hasFrameMarking		= false; CheckExtensionMark(); }
	void  DisableRId()			{ extension.hasRId			= false; CheckExtensionMark(); }
	void  DisableRepairedId()		{ extension.hasRepairedId		= false; CheckExtensionMark(); }
	void  DisableMediaStreamId()		{ extension.hasMediaStreamId		= false; CheckExtensionMark(); }
	void  DisableDependencyDescriptor()	{ extension.hasDependencyDescriptor	= false; CheckExtensionMark(); }
	void  DisablePlayoutDelay()		{ extension.hasPlayoutDelay		= false; CheckExtensionMark(); }
	void  DisableColorSpace()		{ extension.hasColorSpace		= false; CheckExtensionMark(); }
	

	QWORD GetAbsSendTime()			const	{ return extension.absSentTime;			}
	QWORD GetEstimatedAbsSendTime()		const	{ return time % 64000 + extension.absSentTime;  }
	QWORD GetAbsoluteCaptureTime()		const   { return extension.absoluteCaptureTime.GetAbsoluteCaptureTime();	}
	int   GetTimeOffset()			const	{ return extension.timeOffset;			}
	bool  GetVAD()				const	{ return extension.vad;				}
	BYTE  GetLevel()			const	{ return extension.level;			}
	WORD  GetTransportSeqNum()		const	{ return extension.transportSeqNum;		}
	const std::string& GetRId()		const	{ return extension.rid;				}
	const std::string& GetRepairedId()	const	{ return extension.repairedId;			}
	const std::string& GetMediaStreamId()	const	{ return extension.mid;				}
	
	const RTPHeaderExtension::FrameMarks&			GetFrameMarks()			 const { return extension.frameMarks;		}
	const std::optional<DependencyDescriptor>&		GetDependencyDescriptor()	 const { return extension.dependencyDescryptor;	}
	const std::optional<TemplateDependencyStructure>&	GetTemplateDependencyStructure() const { return templateDependencyStructure;	}
	const std::optional<std::vector<bool>>&			GetActiveDecodeTargets()	 const { return activeDecodeTargets;		}
	const VideoOrientation&					GetVideoOrientation()		 const { return extension.cvo;			}
	const struct RTPHeaderExtension::PlayoutDelay&		GetPlayoutDelay()		 const { return extension.playoutDelay;		}
	const std::optional<struct RTPHeaderExtension::ColorSpace>&    GetColorSpace()		 const { return extension.colorSpace;		}
	const std::optional<struct VideoLayersAllocation>&	GetVideoLayersAllocation()	 const { return extension.videoLayersAllocation;}
	
	bool  HasAudioLevel()			const	{ return extension.hasAudioLevel;		}
	bool  HasAbsSentTime()			const	{ return extension.hasAbsSentTime;		}
	bool  HasTimeOffeset()			const   { return extension.hasTimeOffset;		}
	bool  HasTransportWideCC()		const   { return extension.hasTransportWideCC;		}
	bool  HasFrameMarkings()		const   { return extension.hasFrameMarking;		}
	bool  HasRId()				const   { return extension.hasRId;			}
	bool  HasRepairedId()			const   { return extension.hasRepairedId;		}
	bool  HasMediaStreamId()		const   { return extension.hasMediaStreamId;		}
	bool  HasDependencyDestriptor()		const   { return extension.hasDependencyDescriptor;	}
	bool  HasTemplateDependencyStructure()	const	{ return extension.hasDependencyDescriptor &&
								 extension.dependencyDescryptor &&
								 extension.dependencyDescryptor->templateDependencyStructure;	}
	bool  HasVideoOrientation()		const	{ return extension.hasVideoOrientation;		}
	bool  HasAbsoluteCaptureTime()		const	{ return extension.hasAbsoluteCaptureTime;	}
	bool  HasPlayoutDelay()			const   { return extension.hasPlayoutDelay;		}
	bool  HasColorSpace()			const   { return extension.hasColorSpace && extension.colorSpace;		}
	bool  HasVideoLayersAllocation()	const	{ return extension.hasVideoLayersAllocation && extension.videoLayersAllocation; }

	
	void  OverrideActiveDecodeTargets(const std::optional<std::vector<bool>>& activeDecodeTargets) 
	{
		if (extension.dependencyDescryptor)
			extension.dependencyDescryptor->activeDecodeTargets = activeDecodeTargets;
	}
	void OverrideTemplateDependencyStructure(const std::optional<TemplateDependencyStructure>& templateDependencyStructure)
	{
		this->templateDependencyStructure = templateDependencyStructure;
	}
	void  OverrideFrameNumber(uint16_t frameNumber)
	{
		if (extension.dependencyDescryptor)
			extension.dependencyDescryptor->frameNumber = frameNumber;
	}
	
	QWORD GetTime()				const	{ return time;				}
	void  SetTime(QWORD time )			{ this->time = time;			}
	
	QWORD GetSenderTime()			const	{ return senderTime;			}
	void  SetSenderTime(QWORD senderTime )		{ this->senderTime = senderTime;	}
	
	bool  IsKeyFrame()			const	{ return isKeyFrame;			}
	void  SetKeyFrame(bool isKeyFrame)		{ this->isKeyFrame = isKeyFrame;	}
	
	int64_t GetTimestampSkew() const { return timestampSkew; } 
	void  SetTimestampSkew(int64_t timestampSkew) { this->timestampSkew = timestampSkew; } 

	const RTPHeader&		GetRTPHeader()		const { return header;		}
	const RTPHeaderExtension&	GetRTPHeaderExtension()	const { return extension;	}

	uint32_t GetWidth() const	{ return width;			}
	uint32_t GetHeight() const	{ return height;		}
	void SetWidth(uint32_t width)	{ this->width = width;		}
	void SetHeight(uint32_t height) { this->height = height;	}

public:
	//TODO:refactor a bit
	std::optional<VP8PayloadDescriptor>	vp8PayloadDescriptor;
	std::optional<VP8PayloadHeader>		vp8PayloadHeader;
	std::optional<VP9PayloadDescription>	vp9PayloadDescriptor;
	std::optional<H264SeqParameterSet>	h264SeqParameterSet;
	std::optional<H264PictureParameterSet>	h264PictureParameterSet;
	std::optional<std::vector<bool>>	activeDecodeTargets;
	std::optional<TemplateDependencyStructure> templateDependencyStructure;
	Buffer::shared				config;

	bool rewitePictureIds = false;
	
protected:
	void  CheckExtensionMark()	{ header.extension =  extension.hasAudioLevel
						|| extension.hasAbsSentTime 
						|| extension.hasTimeOffset
						|| extension.hasTransportWideCC
						|| extension.hasFrameMarking
						|| extension.hasRId
						|| extension.hasRepairedId
						|| extension.hasMediaStreamId
						|| extension.hasAbsoluteCaptureTime
						|| extension.hasAudioLevel
						|| extension.hasVideoOrientation
						|| extension.hasDependencyDescriptor
						|| extension.hasPlayoutDelay
					; }

private:
	MediaFrame::Type media;

	BYTE		codec;
	DWORD		clockRate;
	WORD		seqCycles	= 0;
	DWORD		timestampCycles	= 0;

	std::optional<WORD> osn;
	
	RTPHeader	   header;
	RTPHeaderExtension extension;
	RTPPayload::shared payload;
	bool ownedPayload		= false;
	QWORD time			= 0;
	bool isKeyFrame			= false;
	QWORD senderTime		= 0;
	int64_t timestampSkew 		= 0;
	uint16_t width			= 0;
	uint16_t height			= 0;
};
#endif /* RTPPACKET_H */

