#include "tracing.h"

#include "rtp/RTPStreamTransponder.h"
#include "waitqueue.h"
#include "vp8/vp8.h"
#include "DependencyDescriptorLayerSelector.h"


RTPStreamTransponder::RTPStreamTransponder(RTPOutgoingSourceGroup* outgoing,RTPSender* sender) :
	timeService(outgoing->GetTimeService())
{
	//Store outgoing streams
	this->outgoing = outgoing;
	this->sender = sender;
	ssrc = outgoing->media.ssrc;
	
	//Add us as listeners
	outgoing->AddListener(this);
	
	Debug("-RTPStreamTransponder() | [outgoing:%p,sender:%p,ssrc:%u]\n",outgoing,sender,ssrc);
}


bool RTPStreamTransponder::SetIncoming(RTPIncomingMediaStream* incoming, RTPReceiver* receiver, bool smooth)
{
	bool res = true;

	timeService.Sync([=,&res](auto now){
		//Check we are not closed
		if (!outgoing)
		{
			//Not updated
			res = false;
			//Error
			Error("-RTPStreamTransponder::SetIncoming() | Transponder already closed\n");
			//Exit
			return;
		}
		
		Debug(">RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p,ssrc:%u,smooth:%d]\n", incoming, receiver, ssrc, smooth);

		if (smooth) 
		{
			//If they are the same as next ones already
			if (this->incomingNext == incoming && this->receiverNext == receiver)
			{
				//Not updated
				res = false;
				//DO nothing
				return;
			}

			//Remove listener from previues transitioning stream
			if (this->incomingNext)
				this->incomingNext->RemoveListener(this);

			//If they are the same as current ones
			if (this->incoming == incoming && this->receiver == receiver)
			{
				//And don't wait anymore
				incomingNext = nullptr;
				receiverNext = nullptr;
				//Not updated
				res = false;
				//DO nothing
				return;
			}

			//Store stream and receiver
			this->incomingNext = incoming;
			this->receiverNext = receiver;

			//Double check
			if (this->incomingNext)
			{
				//Add us as listeners
				this->incomingNext->AddListener(this);

				//Request update on the transitoning one
				if (this->receiverNext) this->receiverNext->SendPLI(this->incomingNext->GetMediaSSRC());
			}

		} else {
			//If we were waiting to transition to a new incoming media stream
			if (this->incomingNext)
			{
				//Stop listening from it
				this->incomingNext->RemoveListener(this);
				//And don't wait anymore
				incomingNext = nullptr;
				receiverNext = nullptr;
			}

			//If they are the same as current ones
			if (this->incoming==incoming && this->receiver==receiver)
			{
				//Not updated
				res = false;
				//DO nothing
				return;
			}
		
	
			//Remove listener from old stream
			if (this->incoming) 
				this->incoming->RemoveListener(this);

			//Reset packets before start listening again
			reset = true;

			//Store stream and receiver
			this->incoming = incoming;
			this->receiver = receiver;
	
			//Double check
			if (this->incoming)
			{
				//Add us as listeners
				this->incoming->AddListener(this);
		
				//Request update on the incoming
				if (this->receiver) this->receiver->SendPLI(this->incoming->GetMediaSSRC());
				//Update last requested PLI
				lastSentPLI = now.count();
			}
		}
	
		Debug("<RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p]\n",incoming,receiver);
	});

	return res;
}

RTPStreamTransponder::~RTPStreamTransponder()
{
	//If not already stopped
	if (outgoing || incoming)
		//Stop listeneing
		Close();
}

void RTPStreamTransponder::Close()
{
	Debug(">RTPStreamTransponder::Close()\n");

	timeService.Sync([=](auto now) {
		//Stop listening
		if (incoming) incoming->RemoveListener(this);
		if (incomingNext) incomingNext->RemoveListener(this);
		//Remove sources
		incoming = nullptr;
		receiver = nullptr;
		incomingNext = nullptr;
		receiverNext = nullptr;
	});
	
	//Stop listening
	if (outgoing) outgoing->RemoveListener(this);
	//Remove sources
	outgoing = nullptr;
	sender = nullptr;
	
	Debug("<RTPStreamTransponder::Close()\n");
}


void RTPStreamTransponder::onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Trace method
	TRACE_EVENT("rtp","RTPStreamTransponder::onRTP", "ssrc", packet->GetSSRC(), "seqnum", packet->GetSeqNum());

	if (!packet)
		//Exit
		return;

	//If it is from the next transitioning stream
	if (stream == incomingNext)
	{
		//If it is a video packet and not an iframe
		if (packet->GetMediaType()==MediaFrame::Video && !packet->IsKeyFrame())
			//Skip
			return;

		//Remove listener from old stream
		if (this->incoming)
			this->incoming->RemoveListener(this);

		//Reset packets
		reset = true;

		//Transition to new stream and receiver
		this->incoming = incomingNext;
		this->receiver = receiverNext;
		this->incomingNext = nullptr;
		this->receiverNext = nullptr;
	}

	//Check if it is from the correct stream
	if (stream!= this->incoming)
		//Skip
		return;
	
	//If muted
	if (muted)
		//Skip
		return;

	//If forwarding only intra frames and video frame is not intra
	if (intraOnlyForwarding && packet->GetMediaType() == MediaFrame::Video && !packet->IsKeyFrame())
	{
		//Drop it
		dropped++;
		//Skip
		return;
	}

	//Check if it is an empty packet
	if (!packet->GetMediaLength())
	{
		UltraDebug("-RTPStreamTransponder::onRTP() | dropping empty packet\n");
		//Drop it
		dropped++;
		//Exit
		return;
	}
	
	//Check sender
	if (!sender)
		//Nothing
		return;
	
	//Check if source has changed
	if (source && packet->GetSSRC()!=source)
		//We need to reset
		reset = true;
	
	//If we need to reset
	if (reset)
	{
		Debug("-StreamTransponder::onRTP() | Reset stream\n");
		//IF last was not completed
		if (!lastCompleted && type==MediaFrame::Video)
		{
			//Create new RTP packet
			RTPPacket::shared rtp = std::make_shared<RTPPacket>(media,codec);
			//Set data
			rtp->SetPayloadType(type);
			rtp->SetSSRC(ssrc);
			rtp->SetExtSeqNum(lastExtSeqNum++);
			rtp->SetMark(true);
			rtp->SetExtTimestamp(lastTimestamp);
			//Send it
			if (sender) sender->Enqueue(rtp);
		}
		//No source
		lastCompleted = true;
		source = 0;
		//Reset first paquet seq num and timestamp
		firstExtSeqNum = NoSeqNum;
		firstTimestamp = NoTimestamp;
		//Store the last send ones
		baseExtSeqNum = lastExtSeqNum+1;
		baseTimestamp = lastTimestamp;
		//None dropped or added
		dropped = 0;
		added = 0;
		//Not selecting
		selector = nullptr;
		//No layer
		spatialLayerId = LayerInfo::MaxLayerId;
		temporalLayerId = LayerInfo::MaxLayerId;
		
		//Reset frame numbers
		firstFrameNumber = NoFrameNum;
		baseFrameNumber = lastFrameNumber + 1;
		frameNumberExtender.Reset();

		//Reseted
		reset = false;
	}
	
	//Update source
	source = packet->GetSSRC();
	//Get new seq number
	DWORD extSeqNum = packet->GetExtSeqNum();
	
	//Check if it the first received packet
	if (firstExtSeqNum==NoSeqNum || firstTimestamp==NoTimestamp)
	{
		//If we have a time offest from last sent packet
		if (lastTime)
		{
			//Calculate time diff
			QWORD offset = getTimeDiff(lastTime)/1000;
			//Get timestamp diff on correct clock rate
			QWORD diff = offset*packet->GetClockRate()/1000;
			
			//UltraDebug("-ts offset:%llu diff:%llu baseTimestap:%lu firstTimestamp:%llu lastTimestamp:%llu rate:%llu\n",offset,diff,baseTimestamp,firstTimestamp,lastTimestamp,packet->GetClockRate());
			
			//convert it to rtp time and add to the last sent timestamp
			baseTimestamp = lastTimestamp + diff + 1;
		}

		//Reset drop counter
		dropped = 0;
		//Store seq number
		firstExtSeqNum = extSeqNum;
		//Get first timestamp
		firstTimestamp = packet->GetExtTimestamp();
		
		UltraDebug("-StreamTransponder::onRTP() | first seq:%lu base:%lu last:%lu ts:%llu baseSeq:%lu baseTimestamp:%llu lastTimestamp:%llu\n",firstExtSeqNum,baseExtSeqNum,lastExtSeqNum,firstTimestamp,baseExtSeqNum,baseTimestamp,lastTimestamp);
	}
	
	//Ensure it is not before first one
	if (extSeqNum<firstExtSeqNum)
		//Exit
		return;
	
	//Only for viedo
	if (packet->GetMediaType()==MediaFrame::Video)
	{
		//Check if we don't have one or if we have a selector and it is not from the same codec
		if (!selector || (BYTE)selector->GetCodec()!=packet->GetCodec())
		{
			//Create new selector for codec
			selector.reset(VideoLayerSelector::Create((VideoCodec::Type)packet->GetCodec()));
			//Set prev layers
			selector->SelectSpatialLayer(spatialLayerId);
			selector->SelectTemporalLayer(temporalLayerId);
		}
	}
	
	//Get rtp marking
	bool mark = packet->GetMark();
	
	//If we have selector for codec
	if (selector)
	{
		//Select layer
		selector->SelectSpatialLayer(spatialLayerId);
		selector->SelectTemporalLayer(temporalLayerId);
		
		//Select pacekt
		if (!packet->GetMediaLength() || !selector->Select(packet,mark))
		{
			//One more dropperd
			dropped++;
			//If selector is waiting for intra and last PLI was more than 1s ago
			if (selector->IsWaitingForIntra() && getTimeDiffMS(lastSentPLI)>1E3)
			{
				//Log
				//UltraDebug("-RTPStreamTransponder::onRTP() | selector IsWaitingForIntra\n");
				//Request it again
				RequestPLI();
			}
			//Drop
			return;
		}
		//Get current spatial layer id
		lastSpatialLayerId = selector->GetSpatialLayer();
	}
	
	//Set normalized seq num
	extSeqNum = baseExtSeqNum + (extSeqNum - firstExtSeqNum) - dropped + added;
	
	//Set normailized timestamp
	uint64_t timestamp = baseTimestamp + (packet->GetExtTimestamp()-firstTimestamp);
	
	//UPdate media codec and type
	media = packet->GetMediaType();
	codec = packet->GetCodec();
	type  = packet->GetPayloadType();
	
	//UltraDebug("-ext seq:%lu base:%lu first:%lu current:%lu dropped:%lu added:%d ts:%lu normalized:%llu intra:%d codec=%d\n",extSeqNum,baseExtSeqNum,firstExtSeqNum,packet->GetExtSeqNum(),dropped,added,packet->GetTimestamp(),timestamp,packet->IsKeyFrame(),codec);
	
	//Rewrite pict id
	bool rewitePictureIds = false;
	DWORD pictureId = 0;
	DWORD temporalLevelZeroIndex = 0;
	//TODO: this should go into the layer selector??
	if (rewritePicId && codec==VideoCodec::VP8 && packet->vp8PayloadDescriptor)
	{
		//Get VP8 desc
		auto desc = *packet->vp8PayloadDescriptor;
		
		//Check if we have a new pictId
		if (desc.pictureIdPresent && desc.pictureId!=lastPicId)
		{
			//Update ids
			lastPicId = desc.pictureId;
			//Increase picture id
			picId++;
		}
		
		//Check if we a new base layer
		if (desc.temporalLevelZeroIndexPresent && desc.temporalLevelZeroIndex!=lastTl0Idx)
		{
			//Update ids
			lastTl0Idx = desc.temporalLevelZeroIndex;
			//Increase tl0 index
			tl0Idx++;
		}
		
		//Rewrite picture id
		pictureId = picId;
		//Rewrite tl0 index
		temporalLevelZeroIndex = tl0Idx;
		//We need to rewrite vp8 picture ids
		rewitePictureIds = true;
	}
	
	//If we have to append h264 sprop parameters set for the first packet of an iframe
	if (h264Parameters && codec==VideoCodec::H264 && packet->IsKeyFrame() && (timestamp!=lastTimestamp || firstExtSeqNum==packet->GetExtSeqNum()))
	{
		//UltraDebug("-addding h264 sprop\n");
		
		//Clone packet
		auto cloned = h264Parameters->Clone();
		//Set new seq numbers
		cloned->SetExtSeqNum(extSeqNum);
		//Set normailized timestamp
		cloned->SetExtTimestamp(timestamp);
		//Set payload type
		cloned->SetPayloadType(type);
		//Change ssrc
		cloned->SetSSRC(ssrc);
		//Send packet
		if (sender)
			//Create clone on sender thread
			sender->Enqueue(cloned);
		//Add new packet
		added ++;
		extSeqNum ++;
		
		//UltraDebug("-ext seq:%lu base:%lu first:%lu current:%lu dropped:%lu added:%d ts:%lu normalized:%llu intra:%d codec=%d\n",extSeqNum,baseExtSeqNum,firstExtSeqNum,packet->GetExtSeqNum(),dropped,added,packet->GetTimestamp(),timestamp,packet->IsKeyFrame(),codec);
	}
	
	//Dependency descriptor active decodte target mask
	std::optional<std::vector<bool>> forwaredDecodeTargets;
	
	//If it is AV1
	if (codec==VideoCodec::AV1)
		//Get decode target
		forwaredDecodeTargets = static_cast<DependencyDescriptorLayerSelector*>(selector.get())->GetForwardedDecodeTargets();

	//Continous frame number
	uint64_t continousFrameNumber = NoFrameNum;
	//Get frame number if we have dependency descriptor
	if (packet->HasDependencyDestriptor())
	{
		//Get it
		auto dd = packet->GetDependencyDescriptor();

		//Double check
		if (dd)
		{
			//Extend it
			frameNumberExtender.Extend(dd->frameNumber);
			//Get extended frame number
			uint64_t frameNumber = frameNumberExtender.GetExtSeqNum();

			//If it is first
			if (firstFrameNumber==NoFrameNum)
				//Set it
				firstFrameNumber = frameNumber;
			//If it is the first frame after reset
			if (baseFrameNumber==NoFrameNum)
				//Set it
				baseFrameNumber = frameNumber;
			//Calculate a continous frame number
			continousFrameNumber = baseFrameNumber + frameNumber - firstFrameNumber;

			//UltraDebug("-frameNum first:%llu base:%llu current:%llu(%u) continous=%llu\n", firstFrameNumber, baseFrameNumber, lastFrameNumber, dd->frameNumber, continousFrameNumber);
		}
	}
	
	//Get last send seq num and timestamp
	lastExtSeqNum = extSeqNum;
	lastTimestamp = timestamp;
	//Update last sent time
	lastTime = getTime();

	//Get last frame number
	lastFrameNumber = continousFrameNumber;
	
	//Send packet
	if (sender)
		//Create clone on sender thread
		sender->Enqueue(packet,[=](const RTPPacket::shared& packet) -> RTPPacket::shared {
			//Trace event
			TRACE_EVENT("rtp","RTPStreamTransponder::onRTP::ClonePacket", "ssrc", packet->GetSSRC(), "seqnum", packet->GetSeqNum());

			//Clone packet
			auto cloned = packet->Clone();
			//Set new seq numbers
			cloned->SetExtSeqNum(extSeqNum);
			//Set normailized timestamp
			cloned->SetTimestamp(timestamp);
			//Set mark again
			cloned->SetMark(mark);
			//Change ssrc
			cloned->SetSSRC(ssrc);
			//We need to rewrite vp8 picture ids
			cloned->rewitePictureIds = rewitePictureIds;
			//Ensure we have desc
			if (cloned->vp8PayloadDescriptor)
			{
				//Rewrite picture id
				cloned->vp8PayloadDescriptor->pictureId = pictureId;
				//Rewrite tl0 index
				cloned->vp8PayloadDescriptor->temporalLevelZeroIndex = temporalLevelZeroIndex;
			}
			//If it has a dependency descriptor
			if (forwaredDecodeTargets && cloned->HasTemplateDependencyStructure())
				//Override mak
				cloned->OverrideActiveDecodeTargets(forwaredDecodeTargets);
			//If we have a continous frame number
			if (cloned->HasDependencyDestriptor() && continousFrameNumber!=NoFrameNum)
				//Update it
				cloned->OverrideFrameNumber(static_cast<uint16_t>(continousFrameNumber));

			//Move it
			return cloned;
		});
}

void RTPStreamTransponder::onBye(RTPIncomingMediaStream* stream)
{
	timeService.Async([=](auto) {
	
		//If they are the not same
		if (this->incoming!=stream)
			//DO nothing
			return;
	
		//Reset packets
		reset = true;
	});
}

void RTPStreamTransponder::onEnded(RTPIncomingMediaStream* stream)
{
	timeService.Sync([=](auto){
		//IF it is the current one
		if (this->incoming == stream)
		{
			//Reset packets before start listening again
			reset = true;
	
			//No stream and receiver
			this->incoming = nullptr;
			this->receiver = nullptr;
		//If it is the next one
		} else if (this->incomingNext == stream) {
			//No transitioning stream and receiver
			this->incomingNext = nullptr;
			this->receiverNext = nullptr;
		}
	});
}
void RTPStreamTransponder::onEnded(RTPOutgoingSourceGroup* group)
{
	timeService.Sync([=](auto) {
		//IF it is the current one
		if (this->outgoing == group)
			//No more outgoing
			this->outgoing = nullptr;
	});
}


void RTPStreamTransponder::RequestPLI()
{
	//Log("-RTPStreamTransponder::RequestPLI() [receiver:%p,incoming:%p]\n",receiver,incoming);
	timeService.Async([=](auto now) {
		//Request update on the incoming
		if (receiver && incoming) receiver->SendPLI(incoming->GetMediaSSRC());
		//Update last sent pli
		lastSentPLI = now.count();
	});
}

void RTPStreamTransponder::onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc)
{
	//Log
	UltraDebug("-RTPStreamTransponder::onPLIRequest()\n");
	RequestPLI();
}

void RTPStreamTransponder::onREMB(RTPOutgoingSourceGroup* group,DWORD ssrc, DWORD bitrate)
{
	UltraDebug("-RTPStreamTransponder::onREMB() [ssrc:%u,bitrate:%u]\n",ssrc,bitrate);
}


void RTPStreamTransponder::SelectLayer(int spatialLayerId,int temporalLayerId)
{
	//Log
	UltraDebug("-RTPStreamTransponder::SelectLayer() | [sid:%d,tid:%d]\n", spatialLayerId, temporalLayerId);

	this->spatialLayerId  = spatialLayerId;
	this->temporalLayerId = temporalLayerId;
	
	if (lastSpatialLayerId!=spatialLayerId)
		//Request update on the incoming
		RequestPLI();
}

void RTPStreamTransponder::Mute(bool muting)
{
	//Log
	UltraDebug("-RTPStreamTransponder::Mute() | [muting:%d]\n", muting);

	//Check if we are changing state
	if (muting==muted)
		//Do nothing
		return;
	//If unmutting
	if (!muting)
		//Request update
		RequestPLI();
	//Update state
	muted = muting;
}

void RTPStreamTransponder::SetIntraOnlyForwarding(bool intraOnlyForwarding)
{
	//Log
	UltraDebug("-RTPStreamTransponder::SetIntraOnlyForwarding() | [intraOnlyForwarding:%d]\n", intraOnlyForwarding);

	//Set flag
	this->intraOnlyForwarding = intraOnlyForwarding;
}

bool RTPStreamTransponder::AppendH264ParameterSets(const std::string& sprop)
{
	
	Debug("-RTPStreamTransponder::AppendH264ParameterSets() [sprop:%s]\n",sprop.c_str());
	
	//Create pakcet
	auto rtp = std::make_shared<RTPPacket>(MediaFrame::Video,VideoCodec::H264);
	
	//Get current length
	BYTE* data = rtp->AdquireMediaData();
	DWORD len = 0;
	DWORD size = rtp->GetMaxMediaLength();
	//Append stap-a header
	data[len++] = 24;
	//Split by ","
	auto start = 0;
	
	//Get next separator
	auto end = sprop.find(',');
	
	//Parse
	while(end!=std::string::npos)
	{
		//Get prop
		auto prop = sprop.substr(start,end-start);
		
		Debug("-RTPStreamTransponder::AppendH264ParameterSets() [sprop:%s]\n",prop.c_str());
		//Parse and keep space for size
		auto l = av_base64_decode(data+len+2,prop.c_str(),size-len-2);
		//Check result
		if (l<=0)
			return Error("-RTPStreamTransponder::AppendH264ParameterSets() could not decode base64 data [%s]\n",prop.c_str());
		//Set naly length
		set2(data,len,l);
		//Increase length
		len += l+2;
		//Next
		start = end+1;
		//Get nest
		end = sprop.find(',',start);
	}
	//last one
	auto prop = sprop.substr(start,end-start);
	
	//Parse and keep space for size
	auto l = av_base64_decode(data+len+2,prop.c_str(),size-len-2);
	//Check result
	if (l<=0)
		return Error("-RTPStreamTransponder::AppendH264ParameterSets() could not decode base64 data [%s]\n",prop.c_str());
	//Set naly length
	set2(data,len,l);
	//Increase length
	len += l+2;
	//Set new lenght
	rtp->SetMediaLength(len);
	
	//Store it
	this->h264Parameters = rtp;
	
	//Done
	return true;
}
