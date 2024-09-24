#ifndef _RTP_H_
#define _RTP_H_
#include <arpa/inet.h>
#include "config.h"
#include "log.h"
#include "media.h"
#include <vector>
#include <list>
#include <utility>
#include <map>
#include <set>
#include <math.h>

#include "rtp/RTPMap.h"
#include "rtp/RTPHeader.h"
#include "rtp/RTPHeaderExtension.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPPacketSched.h"
#include "rtp/RTPRedundantPacket.h"
#include "rtp/RTPDepacketizer.h"
#include "rtp/RTCPReport.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCompoundPacket.h"
#include "rtp/RTCPApp.h"
#include "rtp/RTCPExtendedJitterReport.h"
#include "rtp/RTCPSenderReport.h"
#include "rtp/RTCPBye.h"
#include "rtp/RTCPFullIntraRequest.h"
#include "rtp/RTCPPayloadFeedback.h"
#include "rtp/RTCPRTPFeedback.h"
#include "rtp/RTCPNACK.h"
#include "rtp/RTCPReceiverReport.h"
#include "rtp/RTCPSDES.h"
#include "rtp/RTPWaitedBuffer.h"
#include "rtp/RTPLostPackets.h"
#include "rtp/RTPSource.h"
#include "rtp/RTPIncomingMediaStream.h"
#include "rtp/RTPIncomingMediaStreamMultiplexer.h"
#include "rtp/RTPIncomingSource.h"
#include "rtp/RTPIncomingSourceGroup.h"
#include "rtp/RTPOutgoingSource.h"
#include "rtp/RTPOutgoingSourceGroup.h"

class RTPSender
{
public:
	using shared = std::shared_ptr<RTPSender>;
public:
	virtual int Enqueue(const RTPPacket::shared& packet) = 0;
};

class RTPReceiver
{
public:
	using shared = std::shared_ptr<RTPReceiver>;
public:
	virtual int SendPLI(DWORD ssrc) = 0;
	virtual int Reset(DWORD ssrc) = 0;
};
#endif
