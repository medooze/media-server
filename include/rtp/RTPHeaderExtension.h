#ifndef RTPHEADEREXTENSION_H
#define RTPHEADEREXTENSION_H
#include <optional>
#include <cmath>
#include <vector>

#include "config.h"
#include "tools.h"
#include "rtp/RTPMap.h"
#include "rtp/DependencyDescriptor.h"
#include "rtp/VideoLayersAllocation.h"

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
		AbsoluteCaptureTime	= 11,
		PlayoutDelay		= 12,
		ColorSpace		= 13,
		VideoLayersAllocation	= 14,
		Reserved		= 15
	};
	
	static Type GetExtensionForName(const char* ext)
	{
		if	(strcasecmp(ext,"urn:ietf:params:rtp-hdrext:toffset")==0)						return Type::TimeOffset;
		else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")==0)			return Type::AbsoluteSendTime;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:ssrc-audio-level")==0)					return Type::SSRCAudioLevel;
		else if (strcasecmp(ext,"urn:3gpp:video-orientation")==0)							return Type::CoordinationOfVideoOrientation;
		else if (strcasecmp(ext,"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01")==0)	return Type::TransportWideCC;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:framemarking")==0)						return Type::FrameMarking;
		else if (strcasecmp(ext,"http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07")==0)			return Type::FrameMarking;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id")==0)					return Type::RTPStreamId;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id")==0)				return Type::RepairedRTPStreamId;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:mid")==0)						return Type::MediaStreamId;
		else if (strcasecmp(ext,"https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension")==0) return Type::DependencyDescriptor;
		else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time") == 0)			return Type::AbsoluteCaptureTime;
		else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/playout-delay") == 0)			return Type::PlayoutDelay;
		else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/color-space") == 0)			return Type::ColorSpace;
		else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation00") == 0)		return Type::VideoLayersAllocation;
		return Type::UNKNOWN;
	}

	static const char* GetNameFor(Type ext)
	{
		switch (ext)
		{
			case Type::TimeOffset:				return "TimeOffset";
			case Type::AbsoluteSendTime:			return "AbsoluteSendTime";
			case Type::SSRCAudioLevel:			return "SSRCAudioLevel";
			case Type::CoordinationOfVideoOrientation:	return "CoordinationOfVideoOrientation";
			case Type::TransportWideCC:			return "TransportWideCC";
			case Type::FrameMarking:			return "FrameMarking";
			case Type::RTPStreamId:				return "RTPStreamId";
			case Type::RepairedRTPStreamId:			return "RepairedRTPStreamId";
			case Type::MediaStreamId:			return "MediaStreamId";
			case Type::DependencyDescriptor:		return "DependencyDescriptor";
			case Type::AbsoluteCaptureTime:			return "AbsoluteCaptureTime";
			case Type::PlayoutDelay:			return "PlayoutDelay";
			case Type::ColorSpace:				return "ColorSpace";
			case Type::VideoLayersAllocation:		return "VideoLayersAllocation";
			default:					return "unknown";
		}
	}
	
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

	struct AbsoluteCaptureTime
	{
		uint64_t absoluteCatpureTimestampNTP	= 0;
		int64_t  estimatedCaptureClockOffsetNTP	= 0;

		uint64_t GetAbsoluteCaptureTimestamp() const
		{
			return (absoluteCatpureTimestampNTP + estimatedCaptureClockOffsetNTP) * (1000.0 / 0x100000000);
		}

		uint64_t GetAbsoluteCaptureTime() const
		{
			return absoluteCatpureTimestampNTP ? GetAbsoluteCaptureTimestamp() - 2208988800000ULL : 0;
		}

		void SetAbsoluteCaptureTime(uint64_t ms)
		{
			SetAbsoluteCaptureTimestamp(ms + 2208988800000ULL);
		}

		void SetAbsoluteCaptureTimestamp(uint64_t ntp)
		{
			absoluteCatpureTimestampNTP = std::round(ntp * (0x100000000 / 1000.0));
			estimatedCaptureClockOffsetNTP = 0;

		}
	};

	struct PlayoutDelay
	{
		uint16_t min = 0;	// In ms
		uint16_t max = 0;	// In ms
		PlayoutDelay() = default;
		PlayoutDelay(uint16_t min, uint16_t max) : min(min), max(max)
		{

		}
		void SetPlayoutDelay(uint16_t min, uint16_t max)
		{
			this->min = min;
			this->max = max;
		}

		static constexpr int GranularityMs = 10;
	};
	/*
	* 
	* Data layout without HDR metadata (one-byte RTP header extension) 1-byte header + 4 bytes of data:
	*
	*	  0                   1                   2                   3
	*	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |  ID   | L = 3 |   primaries   |   transfer    |    matrix     |
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |range+chr.sit. |
	*	 +-+-+-+-+-+-+-+-+
	* 
	* Data layout of color space with HDR metadata (two-byte RTP header extension) 2-byte header + 28 bytes of data:
	*
	*	  0                   1                   2                   3
	*	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |      ID       |   length=27   |   primaries   |   transfer    |
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |    matrix     |range+chr.sit. |         luminance_max         |
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |         luminance_min         |            mastering_metadata.|
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |primary_r.x and .y             |            mastering_metadata.|
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |primary_g.x and .y             |            mastering_metadata.|
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |primary_b.x and .y             |            mastering_metadata.|
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 |white.x and .y                 |    max_content_light_level    |
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*	 | max_frame_average_light_level |
	*	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	struct HDRMetadata
	{
		BYTE luminanceMax;
		BYTE luminanceMin;
		BYTE primaryRX;
		BYTE primaryRY;
		BYTE primaryGX;
		BYTE primaryGY;
		BYTE primaryBX;
		BYTE primaryBY;
		BYTE whiteX;
		BYTE whiteY;
		DWORD maxContentLightLevel;
		DWORD maxFrameAverageLightLevel;
	};
	struct ColorSpace
	{
		BYTE primaries;
		BYTE transfer;
		BYTE matrix;
		BYTE range;
		BYTE chromeSitingHorizontal;
		BYTE chromeSitingVertical;

		std::optional<HDRMetadata> hdrMetadata;
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
	struct AbsoluteCaptureTime absoluteCaptureTime;
	struct PlayoutDelay playoutDelay;
	std::optional<struct ColorSpace> colorSpace;
	std::optional<struct VideoLayersAllocation> videoLayersAllocation;
	
	bool	hasAbsSentTime		= false;
	bool	hasTimeOffset		= false;
	bool	hasAudioLevel		= false;
	bool	hasVideoOrientation	= false;
	bool	hasTransportWideCC	= false;
	bool	hasFrameMarking		= false;
	bool	hasRId			= false;
	bool	hasRepairedId		= false;
	bool	hasMediaStreamId	= false;
	bool	hasDependencyDescriptor	= false;
	bool	hasAbsoluteCaptureTime	= false;
	bool	hasPlayoutDelay		= false;
	bool	hasColorSpace		= false;
	bool    hasVideoLayersAllocation= false;
};

#endif /* RTPHEADEREXTENSION_H */

