/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPHeaderExtension.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:51
 */

#ifndef RTPHEADEREXTENSION_H
#define RTPHEADEREXTENSION_H
#include <optional>

#include "config.h"
#include "tools.h"
#include "rtp/RTPMap.h"
#include "rtp/DependencyDescriptor.h"

class RTPHeaderExtension
{
public:
	enum Type {
		UNKNOWN			= 0,
		SSRCAudioLevel		= 1,
		TimeOffset		= 2,
		AbsoluteSendTime	= 3,
		CoordinationOfVideoOrientation	= 4,
		TransportWideCC		= 5,
		FrameMarking		= 6,
		RTPStreamId		= 7,
		RepairedRTPStreamId	= 8,
		MediaStreamId		= 9,
		DependencyDescriptor	= 10,
		Reserved		= 15
	};
	
	static Type GetExtensionForName(const char* ext)
	{
		if	(strcasecmp(ext,"urn:ietf:params:rtp-hdrext:toffset")==0)						return TimeOffset;
		else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")==0)			return AbsoluteSendTime;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:ssrc-audio-level")==0)					return SSRCAudioLevel;
		else if (strcasecmp(ext,"urn:3gpp:video-orientation")==0)							return CoordinationOfVideoOrientation;
		else if (strcasecmp(ext,"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01")==0)	return TransportWideCC;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:framemarking")==0)						return FrameMarking;
		else if (strcasecmp(ext,"http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07")==0)			return FrameMarking;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id")==0)					return RTPStreamId;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id")==0)				return RepairedRTPStreamId;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:mid")==0)						return MediaStreamId;
		else if (strcasecmp(ext,"https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension")==0) return DependencyDescriptor;
		return UNKNOWN;
	}

	static const char* GetNameFor(Type ext)
	{
		switch (ext)
		{
			case TimeOffset:			return "TimeOffset";
			case AbsoluteSendTime:			return "AbsoluteSendTime";
			case SSRCAudioLevel:			return "SSRCAudioLevel";
			case CoordinationOfVideoOrientation:	return "CoordinationOfVideoOrientation";
			case TransportWideCC:			return "TransportWideCC";
			case FrameMarking:			return "FrameMarking";
			case RTPStreamId:			return "RTPStreamId";
			case RepairedRTPStreamId:		return "RepairedRTPStreamId";
			case MediaStreamId:			return "MediaStreamId";
			case DependencyDescriptor:		return "DependencyDescriptor";
			default:				return "unknown";
		}
	}
	
	struct VideoOrientation
	{
		bool facing	= 0;
		bool flip	= 0;
		BYTE rotation	= 0;
	};
	
	// For Frame Marking RTP Header Extension:
	// https://tools.ietf.org/html/draft-ietf-avtext-framemarking-04#page-4
	// This extensions provides meta-information about the RTP streams outside the
	// encrypted media payload, an RTP switch can do codec-agnostic
	// selective forwarding without decrypting the payload
	//   o  S: Start of Frame (1 bit) - MUST be 1 in the first packet in a
	//      frame; otherwise MUST be 0.
	//   o  E: End of Frame (1 bit) - MUST be 1 in the last packet in a frame;
	//      otherwise MUST be 0.
	//   o  I: Independent Frame (1 bit) - MUST be 1 for frames that can be
	//      decoded independent of prior frames, e.g. intra-frame, VPX
	//      keyframe, H.264 IDR [RFC6184], H.265 IDR/CRA/BLA/RAP [RFC7798];
	//      otherwise MUST be 0.
	//   o  D: Discardable Frame (1 bit) - MUST be 1 for frames that can be
	//      discarded, and still provide a decodable media stream; otherwise
	//      MUST be 0.
	//   o  B: Base Layer Sync (1 bit) - MUST be 1 if this frame only depends
	//      on the base layer; otherwise MUST be 0.  If no scalability is
	//      used, this MUST be 0.
	//   o  TID: Temporal ID (3 bits) - The base temporal layer starts with 0,
	//      and increases with 1 for each higher temporal layer/sub-layer.  If
	//      no scalability is used, this MUST be 0.
	//   o  LID: Layer ID (8 bits) - Identifies the spatial and quality layer
	//      encoded.  If no scalability is used, this MUST be 0 or omitted.
	//      When omitted, TL0PICIDX MUST also be omitted.
	//   o  TL0PICIDX: Temporal Layer 0 Picture Index (8 bits) - Running index
	//      of base temporal layer 0 frames when TID is 0.  When TID is not 0,
	//      this indicates a dependency on the given index.  If no scalability
	//      is used, this MUST be 0 or omitted.  When omitted, LID MUST also
	//      be omitted.
	struct FrameMarks 
	{
		bool startOfFrame	= false;
		bool endOfFrame		= false;
		bool independent	= false;
		bool discardable	= false;
		bool baseLayerSync	= false;
		BYTE temporalLayerId	= 0;
		BYTE layerId		= 0;
		BYTE tl0PicIdx		= 0;
	};
	
public:
	DWORD Parse(const RTPMap &extMap,const BYTE* data,const DWORD size);
	bool  ParseDependencyDescriptor(const std::optional<TemplateDependencyStructure>& templateDependencyStructure);
	DWORD Serialize(const RTPMap &extMap,BYTE* data,const DWORD size) const;
	void  Dump() const;
public:
	QWORD	absSentTime	= 0;
	int	timeOffset	= 0;
	bool	vad		= 0;
	BYTE	level		= 0;
	WORD	transportSeqNum	= 0;
	VideoOrientation cvo;
	FrameMarks frameMarks;
	std::string rid;
	std::string repairedId;
	std::string mid;
	BitReader dependencyDescryptorReader; 
	std::optional<::DependencyDescriptor> dependencyDescryptor;
	
	bool    hasAbsSentTime		= false;
	bool	hasTimeOffset		= false;
	bool	hasAudioLevel		= false;
	bool	hasVideoOrientation	= false;
	bool	hasTransportWideCC	= false;
	bool	hasFrameMarking		= false;
	bool	hasRId			= false;
	bool	hasRepairedId		= false;
	bool	hasMediaStreamId	= false;
	bool	hasDependencyDescriptor	= false;
};

#endif /* RTPHEADEREXTENSION_H */

