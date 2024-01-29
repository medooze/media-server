#include "rtp/RTPIncomingSource.h"

RTCPReport::shared RTPIncomingSource::CreateReport(QWORD now)
{
	//If we have received somthing
	if (!totalPacketsSinceLastSR || !(extSeqNum>=minExtSeqNumSinceLastSR))
		//Nothing to report
		return NULL;
	
	//Get number of total packtes
	DWORD total = extSeqNum - minExtSeqNumSinceLastSR + 1;
	//Calculate lost
	DWORD lostPacketsSinceLastSR = total - totalPacketsSinceLastSR;
	//Add to total lost count
	lostPackets += lostPacketsSinceLastSR;
	//Calculate fraction lost
	DWORD frac = (lostPacketsSinceLastSR*256)/total;

	//Calculate time since last report
	DWORD delaySinceLastSenderReport = 0;
	//Double check it is correct 
	if (lastReceivedSenderReport && now > lastReceivedSenderReport)
		//Get diff in ms
		delaySinceLastSenderReport = (now - lastReceivedSenderReport)/1000;
	//Create report
	RTCPReport::shared report = std::make_shared<RTCPReport>();

	//Set SSRC of incoming rtp stream
	report->SetSSRC(ssrc);

	//Get time and update it
	report->SetDelaySinceLastSRMilis(delaySinceLastSenderReport);
	// The middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) 
	// received as part of the most recent RTCP sender report (SR) packet from source SSRC_n.
	// If no SR has been received yet, the field is set to zero.
	//Other data
	report->SetLastSR(lastReceivedSenderNTPTimestamp >> 16);
	report->SetFractionLost(frac);
	report->SetLastJitter(jitter);
	report->SetLostCount(lostPackets);
	report->SetLastSeqNum(extSeqNum);

	//Reset data
	lastReport = now;
	totalPacketsSinceLastSR = 0;
	totalBytesSinceLastSR = 0;
	minExtSeqNumSinceLastSR = RTPPacket::MaxExtSeqNum;

	//Return it
	return report;
}

RTPIncomingSource::RTPIncomingSource() : 
	RTPSource(),
	acumulatorFrames(1000),
	acumulatorFrameDelay(1000),
	acumulatorCaptureDelay(1000),
	acumulatorLostPackets(1000)
{
	numFrames			= 0;
	numFramesDelta			= 0;
	lostPackets			= 0;
	lostPacketsDelta		= 0;
	lostPacketsGapCount		= 0;
	lostPacketsMaxGap		= 0;
	dropPackets			= 0;
	totalPacketsSinceLastSR		= 0;
	totalBytesSinceLastSR		= 0;
	lostPacketsSinceLastSR		= 0;
	lastReceivedSenderNTPTimestamp	= 0;
	lastReceivedSenderTime		= 0;
	lastReceivedSenderReport	= 0;
	lastReport			= 0;
	lastPLI				= 0;
	totalPLIs			= 0;
	totalNACKs			= 0;
	lastNACKed			= 0;
	lastTimestamp			= 0;
	lastTime			= 0;
	firstReceivedSenderTime		= 0;
	firstReceivedSenderTimestamp	= 0;
	skew				= 0;
	drift				= 1;
	lastCaptureTime			= 0;
	lastCaptureTimestamp		= 0;
	frameDelay			= 0;
	frameDelayMax			= 0;
	frameCaptureDelay		= 0;
	frameCaptureDelayMax		= 0;
	lastCaptureTimestamp		= 0;
	aggregatedLayers		= false;
	width				= 0;
	height				= 0;
	minExtSeqNumSinceLastSR		= RTPPacket::MaxExtSeqNum;
}


void RTPIncomingSource::Reset()
{
	RTPSource::Reset();
	numFrames			= 0;
	numFramesDelta			= 0;
	lostPackets			= 0;
	lostPacketsDelta		= 0;
	lostPacketsGapCount		= 0;
	lostPacketsMaxGap		= 0;	
	dropPackets			= 0;
	totalPacketsSinceLastSR		= 0;
	totalBytesSinceLastSR		= 0;
	lostPacketsSinceLastSR		= 0;
	lastReceivedSenderNTPTimestamp  = 0;
	lastReceivedSenderTime		= 0;
	lastReceivedSenderReport	= 0;
	lastReport			= 0;
	lastPLI				= 0;
	totalPLIs			= 0;
	lastNACKed			= 0;
	lastTimestamp			= 0;
	lastTime			= 0;
	firstReceivedSenderTime		= 0;
	firstReceivedSenderTimestamp	= 0;
	skew				= 0;
	drift				= 1;
	lastCaptureTime			= 0;
	lastCaptureTimestamp		= 0;
	frameDelay			= 0;
	frameDelayMax			= 0;
	frameCaptureDelay		= 0;
	frameCaptureDelayMax		= 0;
	aggregatedLayers		= false;
	width = 0;
	height = 0;
	minExtSeqNumSinceLastSR		= RTPPacket::MaxExtSeqNum;

	timestampExtender.Reset();
	lastReceivedSenderRTPTimestampExtender.Reset();
	targetBitrate.reset();
	targetWidth.reset();
	targetHeight.reset();
	targetFps.reset();
}

DWORD RTPIncomingSource::ExtendTimestamp(DWORD timestamp)
{
	//Set timestamp
	return timestampExtender.Extend(timestamp);
}

WORD RTPIncomingSource::ExtendSeqNum(WORD seqNum)
{
	//Update seq num
	auto cycles = SetSeqNum(seqNum);
	//Check if it is the min for this SR
	if (extSeqNum<minExtSeqNumSinceLastSR)
		//Store minimum
		minExtSeqNumSinceLastSR = extSeqNum;
	//OK
	return cycles; 
}

void RTPIncomingSource::Update(QWORD now,DWORD seqNum,DWORD size,DWORD overheadSize,const std::vector<LayerInfo> &layerInfos, bool aggreagtedLayers, const std::optional<struct VideoLayersAllocation>& videoLayersAllocation)
{
	//Update source normally
	RTPIncomingSource::Update(now, seqNum, size, overheadSize);
	//Set aggregated layers flag
	this->aggregatedLayers = aggreagtedLayers;
	//For each layer
	for (const auto& layerInfo : layerInfos)
	{
		//Check layer info is present
		if (layerInfo.IsValid())
		{
			//Insert layer info if it doesn't exist
			auto [it, inserted] = layers.try_emplace(layerInfo.GetId(), layerInfo);
			//Update layer source
			it->second.Update(now, size, overheadSize);
		}
	}
	//If packet has VLA header extension 
	if (videoLayersAllocation)
	{
		videoLayersAllocation->Dump();
		//Deactivate all layers
		for (auto& [layerId, layerSource] : layers)
			layerSource.active = false;

		//For each active layer
		for (const auto& activeLayer : videoLayersAllocation->activeSpatialLayers)
		{
			//IF it is from us
			if (activeLayer.streamIdx == videoLayersAllocation->streamIdx)
			{
				bool found = false;
				//Find layer
				for (auto& [layerId,layerSource] : layers)
				{
					//if found
					if (layerSource.spatialLayerId == activeLayer.spatialId || layerSource.spatialLayerId == LayerInfo::MaxLayerId)
					{
						//It is active
						layerSource.active = true;
						//Get bitrate for temporal layer, in bps
						layerSource.targetBitrate = activeLayer.targetBitratePerTemporalLayer[layerSource.temporalLayerId] * 1000;
						//Set dimensios for the spatial layer
						layerSource.targetWidth = activeLayer.width;
						layerSource.targetHeight = activeLayer.height;
						layerSource.targetFps = activeLayer.fps;
						//Layer found
						found = true;
					}
				}
				//If layer was not found on the layer info or ther was no layer info
				if (!found)
				{
					//If we don't have any layer and the vla is coming from the base layer, do not create a new one
					if (layers.empty() && activeLayer.targetBitratePerTemporalLayer.size() == 1 && activeLayer.spatialId == 0)
					{
						//Get bitrate for temporal layer, in bps
						this->targetBitrate = activeLayer.targetBitratePerTemporalLayer[0] * 1000;
						//Set dimensios for the spatial layer
						this->targetWidth = activeLayer.width;
						this->targetHeight = activeLayer.height;
						this->targetFps = activeLayer.fps;

					} else {
						//For each temporal layer
						for (size_t temporalLayerId = 0; temporalLayerId < activeLayer.targetBitratePerTemporalLayer.size(); ++temporalLayerId)
						{
							//Create new Layer
							LayerInfo layerInfo(temporalLayerId, activeLayer.spatialId);
							//Insert layer info if it doesn't exist
							auto [it, inserted] = layers.try_emplace(layerInfo.GetId(), layerInfo);
							//Update layer source
							it->second.Update(now, size, overheadSize);
							//It is active
							it->second.active = true;
							//Get bitrate for temporal layer, in bps
							it->second.targetBitrate = activeLayer.targetBitratePerTemporalLayer[temporalLayerId] * 1000;
							//Set dimensios for the spatial layer
							it->second.targetWidth = activeLayer.width;
							it->second.targetHeight = activeLayer.height;
							it->second.targetFps = activeLayer.fps;
						}
					}
				}
			}
		}
	}
}

void RTPIncomingSource::Update(QWORD now,DWORD seqNum,DWORD size,DWORD overheadSize)
{
	//Store last seq number before updating
	//DWORD lastExtSeqNum = extSeqNum;

	//Update source
	RTPSource::Update(now,seqNum,size,overheadSize);

	totalPacketsSinceLastSR++;
	totalBytesSinceLastSR += size + overheadSize;
	
	//TODO: remove, this should be redundant
	SetSeqNum(seqNum);

	
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

void RTPIncomingSource::Update(QWORD now)
{
	//Update source normally
	RTPSource::Update(now);
	//Update also all media layers
	for (auto& [layerId,layer] : layers)
		//Update bitrate also
		layer.Update(now);
	//Update deltas
	numFramesDelta	 = acumulatorFrames.Update(now);
	lostPacketsDelta = acumulatorLostPackets.Update(now);
	lostPacketsGapCount = acumulatorLostPackets.GetCount();
	lostPacketsMaxGap = acumulatorLostPackets.GetMaxValueInWindow();

	//Update delay accuumulators
	acumulatorFrameDelay.Update(now);
	acumulatorCaptureDelay.Update(now);
	//Get max and averages
	frameDelay		= acumulatorFrameDelay.GetInstantMedia();
	frameDelayMax		= acumulatorFrameDelay.GetMaxValueInWindow();
	frameCaptureDelay	= acumulatorCaptureDelay.GetInstantMedia();
	frameCaptureDelayMax	= acumulatorCaptureDelay.GetMaxValueInWindow();

	//UltraDebug("-RTPIncomingSource::Update() [frameDelay:%d,frameDelayMax:%d,frameDelayMax:%d,frameCaptureDelayMax:%d]\n", frameDelay, frameDelayMax, frameCaptureDelay, frameCaptureDelayMax);
}

void RTPIncomingSource::Process(QWORD now, const RTCPSenderReport::shared& sr)
{
	//If first
	if (!firstReceivedSenderTime)
	{
		//Store time
		firstReceivedSenderTime = sr->GetTimestamp()/1000;
		firstReceivedSenderTimestamp = sr->GetRTPTimestamp();
		//Debug
		UltraDebug("-RTPIncomingSource::Process() | Got first Report [ssrc:0x%x,firstTime:%lld,firstTimestamp:%lld]\n", ssrc, firstReceivedSenderTime, firstReceivedSenderTimestamp);
	}

	//Store info
	lastReceivedSenderNTPTimestamp = sr->GetNTPTimestamp();
	lastReceivedSenderTime = sr->GetTimestamp()/1000;
	lastReceivedSenderRTPTimestampExtender.ExtendOrReset(sr->GetRTPTimestamp());
	lastReceivedSenderReport = now;
	
	//Ensure we have clock rate configured
	if (clockrate)
	{
		//Get diff in sender time
		QWORD deltaTimeMs = (lastReceivedSenderTime-firstReceivedSenderTime);
		QWORD deltaTimeThroughTimestampMs = (lastReceivedSenderRTPTimestampExtender.GetExtSeqNum()-firstReceivedSenderTimestamp)*1000/clockrate;
		//Calculate skew
		skew = deltaTimeMs - deltaTimeThroughTimestampMs;
		drift = deltaTimeMs ? (double)deltaTimeThroughTimestampMs/deltaTimeMs : 1;
		//Debug
		UltraDebug("-RTPIncomingSource::Process() | Sender Report [ssrc:0x%x,skew:%lld,deltaTime:%llu,deltaTimestamp:%llu,senderTime:%llu,firstTime:%lld,firstTimestamp:%lld,rtpTimestamp:%d,lastExtSeqNum:%llu,clockrate:%u]\n",ssrc,skew,deltaTimeMs,deltaTimeThroughTimestampMs,lastReceivedSenderTime, firstReceivedSenderTime, firstReceivedSenderTimestamp, sr->GetRTPTimestamp(), lastReceivedSenderRTPTimestampExtender.GetExtSeqNum(),clockrate);
	}
}

/*
 * Get seq num cycles from a past sequence numer
 */
WORD RTPIncomingSource::RecoverSeqNum(WORD osn)
{
	 //Check secuence wrap
	if ((extSeqNum & 0xFFFF)<0x0FFF && (osn>0xF000))
		//It is from the past cycle
		return cycles - 1;
	//It is from current cyle
	return cycles;
}

/*
 * Get seq num cycles from a past sequence numer
 */
DWORD RTPIncomingSource::RecoverTimestamp(DWORD timestamp)
{
	return timestampExtender.RecoverCycles(timestamp);
}

void RTPIncomingSource::SetLastTimestamp(QWORD now, QWORD timestamp, QWORD captureTimestamp)
{
	//If new packet is newer
	if (timestamp>lastTimestamp)
	{
		//UltraDebug("RTPIncomingSource::SetLastTimestamp() time:[%llu,%llu] timestamp:[%llu,%llu] captureTimestamp:[%llu,%llu]\n", now,lastTime,timestamp,lastTimestamp, captureTimestamp, lastCaptureTimestamp);

		//If we have capture timestamp
		if (captureTimestamp)
		{
			//Calculate e2e delay
			int64_t delay = now - captureTimestamp;

			//e2e delay
			acumulatorCaptureDelay.Update(now, delay);

			//Update stats
			frameCaptureDelay = acumulatorCaptureDelay.GetInstantMedia();
			frameCaptureDelayMax = acumulatorCaptureDelay.GetMaxValueInWindow();

			//UltraDebug("RTPIncomingSource::SetLastTimestamp() [now:%llu,captureTimestamp:%llu,delay:%lld]\n", now, captureTimestamp, delay);

			//If not first one
			if (lastCaptureTimestamp && lastCaptureTimestamp <= captureTimestamp)
			{
				//Get difference between the first packet of a frame in both capture and reception time
				int64_t catpureTimestampDiff = captureTimestamp - lastCaptureTimestamp;
				int64_t receptionTimeDiff = now - lastCaptureTime;
				int64_t interarraivalDelay = receptionTimeDiff - catpureTimestampDiff;

				//UltraDebug("RTPIncomingSource::SetLastTimestamp() [capture:%llu,reception:%llu,delay:%lld]\n", catpureTimestampDiff, receptionTimeDiff, interarraivalDelay);

				//Update accumulators
				acumulatorFrameDelay.Update(now, interarraivalDelay);

				//Update stats
				frameDelay = acumulatorFrameDelay.GetInstantMedia();
				frameDelayMax = acumulatorFrameDelay.GetMaxValueInWindow();
			}

			//UltraDebug("-RTPIncomingSource::SetLastTimestamp() [frameDelay:%d,frameDelayMax:%d,frameDelayMax:%d,frameCaptureDelayMax:%d]\n", frameDelay, frameDelayMax, frameCaptureDelay, frameCaptureDelayMax);

			//Store last capture time
			lastCaptureTimestamp = captureTimestamp;
			lastCaptureTime = now;
		}


		//One new frame
		numFrames++;
		numFramesDelta = acumulatorFrames.Update(now, 1);
		//Store last time
		lastTimestamp = timestamp;
		
	}
	lastTime = now;
}
