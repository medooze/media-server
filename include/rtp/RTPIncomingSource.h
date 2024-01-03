#ifndef RTPINCOMINGSOURCE_H
#define RTPINCOMINGSOURCE_H

#include <map>

#include "config.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPSource.h"
#include "rtp/RTCPReport.h"
#include "acumulator.h"
#include "WrapExtender.h"

struct RTPIncomingSource : public RTPSource
{
	DWORD	numFrames;
	DWORD	numFramesDelta;
	DWORD	lostPackets;
	DWORD	lostPacketsDelta;
	DWORD   lostPacketsGapCount;
	DWORD   lostPacketsMaxGap;
	DWORD	dropPackets;
	DWORD	totalPacketsSinceLastSR;
	DWORD	totalBytesSinceLastSR;
	DWORD	minExtSeqNumSinceLastSR ;
	DWORD   lostPacketsSinceLastSR;
	QWORD   lastReceivedSenderNTPTimestamp;
	QWORD   lastReceivedSenderTime;
	QWORD   lastReceivedSenderReport;
	QWORD   lastReport;
	QWORD	lastPLI;
	DWORD   totalPLIs;
	DWORD	totalNACKs;
	QWORD	lastNACKed;
	QWORD	lastTime;
	QWORD	lastTimestamp;
	QWORD	lastCaptureTime;
	QWORD	lastCaptureTimestamp;
	QWORD   firstReceivedSenderTime;
	QWORD   firstReceivedSenderTimestamp;

	
	int32_t frameDelay;
	int32_t frameDelayMax;
	int32_t frameCaptureDelay;
	int32_t frameCaptureDelayMax;

	int64_t skew;
	double  drift;
	bool	aggregatedLayers;

	uint16_t width;
	uint16_t height;
	std::optional<uint32_t> targetBitrate;
	std::optional<uint16_t> targetWidth;
	std::optional<uint16_t> targetHeight;
	std::optional<uint8_t>  targetFps;

	WrapExtender<uint32_t,uint64_t> timestampExtender;
	WrapExtender<uint32_t,uint64_t> lastReceivedSenderRTPTimestampExtender;
	std::map<WORD,LayerSource> layers;


	
	Acumulator<uint32_t, uint64_t>	  acumulatorFrames;
	MaxAcumulator<int32_t, int64_t>   acumulatorFrameDelay;
	MaxAcumulator<int32_t, int64_t>   acumulatorCaptureDelay;
	MaxAcumulator<uint32_t, uint64_t> acumulatorLostPackets;
	
	RTPIncomingSource();
	virtual ~RTPIncomingSource() = default;
	
	WORD  ExtendSeqNum(WORD seqNum);
	WORD  RecoverSeqNum(WORD osn);
	
	DWORD ExtendTimestamp(DWORD timestamp);
	DWORD RecoverTimestamp(DWORD timestamp);
	
	void Update(QWORD now,DWORD seqNum,DWORD size,DWORD overheadSize,const std::vector<LayerInfo> &layerInfos, bool aggreagtedLayers, const std::optional<struct VideoLayersAllocation>& videoLayersAllocation);
	
	void Process(QWORD now, const RTCPSenderReport::shared& sr);
	void SetLastTimestamp(QWORD now, QWORD timestamp, QWORD captureTimestamp = 0);
	
	virtual void Update(QWORD now,DWORD seqNum,DWORD size,DWORD overheadSize) override;
	virtual void Update(QWORD now) override;
	virtual void Reset() override;
	
	void AddLostPackets(QWORD now, DWORD lost) 
	{
		lostPacketsDelta = acumulatorLostPackets.Update(now, lost); 
	}
	
	RTCPReport::shared CreateReport(QWORD now);
};

#endif /* RTPINCOMINGSOURCE_H */

