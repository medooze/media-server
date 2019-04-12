#ifndef RTPINCOMINGSOURCE_H
#define RTPINCOMINGSOURCE_H

#include <map>

#include "config.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPSource.h"
#include "rtp/RTCPReport.h"

struct RTPIncomingSource : public RTPSource
{
	DWORD	lostPackets;
	DWORD	dropPackets;
	DWORD	totalPacketsSinceLastSR;
	DWORD	totalBytesSinceLastSR;
	DWORD	minExtSeqNumSinceLastSR ;
	DWORD   lostPacketsSinceLastSR;
	QWORD   lastReceivedSenderNTPTimestamp;
	QWORD   lastReceivedSenderReport;
	QWORD   lastReport;
	QWORD	lastPLI;
	DWORD   totalPLIs;
	DWORD	totalNACKs;
	QWORD	lastNACKed;
	DWORD	lastTimestamp;
	QWORD	lastTime;
	std::map<WORD,LayerSource> layers;
	
	RTPIncomingSource() : RTPSource()
	{
		lostPackets		 = 0;
		dropPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lostPacketsSinceLastSR   = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		lastReport		 = 0;
		lastPLI			 = 0;
		totalPLIs		 = 0;
		totalNACKs		 = 0;
		lastNACKed		 = 0;
		lastTimestamp		 = 0;
		lastTime		 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	
	void Update(QWORD now,DWORD seqNum,DWORD size,const LayerInfo &layerInfo)
	{
		//Update source normally
		RTPIncomingSource::Update(now,seqNum,size);
		//Check layer info is present
		if (layerInfo.IsValid())
		{
			//Find layer
			auto it = layers.find(layerInfo.GetId());
			//If not found
			if (it==layers.end())
				//Add new entry
				it = layers.emplace(layerInfo.GetId(),LayerSource(layerInfo)).first;
			//Update layer source
			it->second.Update(now,size);
		}
	}
	
	virtual void Update(QWORD now,DWORD seqNum,DWORD size)
	{
		//Store last seq number before updating
		//DWORD lastExtSeqNum = extSeqNum;
			
		//Update source
		RTPSource::Update(now,seqNum,size);
		
		//Update seq num
		SetSeqNum(seqNum);
		
		totalPacketsSinceLastSR++;
		totalBytesSinceLastSR += size;

		//Check if it is the min for this SR
		if (extSeqNum<minExtSeqNumSinceLastSR)
			//Store minimum
			minExtSeqNumSinceLastSR = extSeqNum;
		
		/*TODO: calculate jitter
		//If we have a not out of order pacekt
		if (lastExtSeqNum != extSeqNum)
		{
			//If it is not first one and not from the same frame
			if (lastTimestamp && lastTimestamp<timestamp)
			{
				//Get diff from previous
				QWORD diff = (lastTime-now)/1000;

		
				//Get difference of arravail times
				int d = (timestamp-lastTimestamp)-diff;
				//Check negative
				if (d<0)
					//Calc abs
					d = -d;
				//Calculate variance
				int v = d - jitter;
				//Calculate jitter
				jitter += v/16;
			}
			//Update rtp timestamp
			lastTime = timestamp;
		}
		 */
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		lostPackets		 = 0;
		dropPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lostPacketsSinceLastSR   = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		lastReport		 = 0;
		lastPLI			 = 0;
		totalPLIs		 = 0;
		lastNACKed		 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	virtual ~RTPIncomingSource() = default;
	
	RTCPReport::shared CreateReport(QWORD now);
};

#endif /* RTPINCOMINGSOURCE_H */

