#include "rtp/RTPStreamTransponder.h"
#include "waitqueue.h"
#include "vp8/vp8.h"
#include "DependencyDescriptorLayerSelector.h"


RTPStreamTransponder::RTPStreamTransponder(RTPOutgoingSourceGroup* outgoing,RTPSender* sender) :
	mutex(true)
{
	//Store outgoing streams
	this->outgoing = outgoing;
	this->sender = sender;
	ssrc = outgoing->media.ssrc;
	
	//Add us as listeners
	outgoing->AddListener(this);
	
	Debug("-RTPStreamTransponder() | [outgoing:%p,sender:%p,ssrc:%u]\n",outgoing,sender,ssrc);
}


bool RTPStreamTransponder::SetIncoming(RTPIncomingMediaStream* incoming, RTPReceiver* receiver)
{
	//If they are the same
	if (this->incoming==incoming && this->receiver==receiver)
		//DO nothing
		return false;
	
	Debug(">RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p,ssrc:%u]\n",incoming,receiver,ssrc);
	
	//Remove listener from old stream
	if (this->incoming) 
		this->incoming->RemoveListener(this);

        //Locked
        {
                ScopedLock lock(mutex);
        
                //Reset packets before start listening again
                reset = true;

                //Store stream and receiver
                this->incoming = incoming;
                this->receiver = receiver;
        }
	
	//Double check
	if (this->incoming)
	{
		//Add us as listeners
		this->incoming->AddListener(this);
		
		//Request update on the incoming
		if (this->receiver) this->receiver->SendPLI(this->incoming->GetMediaSSRC());
		//Update last requested PLI
		lastSentPLI = getTime();
	}
	
	Debug("<RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p]\n",incoming,receiver);
	
	//OK
	return true;
}

RTPStreamTransponder::~RTPStreamTransponder()
{
	//Stop listeneing
	Close();
}

void RTPStreamTransponder::Close()
{
	Debug(">RTPStreamTransponder::Close()\n");
	
	//Stop listeneing
	if (outgoing) outgoing->RemoveListener(this);
	if (incoming) incoming->RemoveListener(this);

	//Lock
	ScopedLock lock(mutex);

	//Remove sources
	outgoing = nullptr;
	incoming = nullptr;
	receiver = nullptr;
	sender   = nullptr;
	
	Debug("<RTPStreamTransponder::Close()\n");
}


void RTPStreamTransponder::onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	
	if (!packet)
		//Exit
		return;
	
	//If muted
	if (muted)
		//Skip
		return;

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
		firstExtSeqNum = 0;
		firstTimestamp = 0;
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
		
		//Reseted
		reset = false;
	}
	
	//Update source
	source = packet->GetSSRC();
	//Get new seq number
	DWORD extSeqNum = packet->GetExtSeqNum();
	
	//Check if it the first received packet
	if (!firstExtSeqNum)
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
		} else {
			//Ini from 0 (We could random, but who cares, this way it is easier to debug)
			baseTimestamp = 0;
		}
		//Store seq number
		firstExtSeqNum = extSeqNum;
		//Store the last send ones as base for new stream
		baseExtSeqNum = lastExtSeqNum+1;
		//Reset drop counter
		dropped = 0;
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
			if (selector->IsWaitingForIntra() && getTimeDiff(lastSentPLI)>1E6)
				//Request it again
				RequestPLI();
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
	
	//Get last send seq num and timestamp
	lastExtSeqNum = extSeqNum;
	lastTimestamp = timestamp;
	//Update last sent time
	lastTime = getTime();
	
	//Send packet
	if (sender)
		//Create clone on sender thread
		sender->Enqueue(packet,[=](const RTPPacket::shared& packet) -> RTPPacket::shared {
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
			//Move it
			return std::move(cloned);
		});
}

void RTPStreamTransponder::onBye(RTPIncomingMediaStream* stream)
{
	ScopedLock lock(mutex);
	
	//If they are the not same
	if (this->incoming!=stream)
		//DO nothing
		return;
	
	//Reset packets
	reset = true;
}

void RTPStreamTransponder::onEnded(RTPIncomingMediaStream* stream)
{
	ScopedLock lock(mutex);
	
	//If they are the not same
	if (this->incoming!=stream)
		//DO nothing
		return;
	
	//Reset packets before start listening again
	reset = true;
	
	//Store stream and receiver
	this->incoming = nullptr;
	this->receiver = nullptr;
}

void RTPStreamTransponder::RequestPLI()
{
	//Log("-RTPStreamTransponder::RequestPLI() [receiver:%p,incoming:%p]\n",receiver,incoming);
	ScopedLock lock(mutex);
	//Request update on the incoming
	if (receiver && incoming) receiver->SendPLI(incoming->GetMediaSSRC());
	//Update last sent pli
	lastSentPLI = getTime();
}

void RTPStreamTransponder::onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc)
{
	RequestPLI();
}

void RTPStreamTransponder::onREMB(RTPOutgoingSourceGroup* group,DWORD ssrc, DWORD bitrate)
{
	UltraDebug("-RTPStreamTransponder::onREMB() [ssrc:%u,bitrate:%u]\n",ssrc,bitrate);
}


void RTPStreamTransponder::SelectLayer(int spatialLayerId,int temporalLayerId)
{
	this->spatialLayerId  = spatialLayerId;
	this->temporalLayerId = temporalLayerId;
	
	if (lastSpatialLayerId!=spatialLayerId)
		//Request update on the incoming
		RequestPLI();
}

void RTPStreamTransponder::Mute(bool muting)
{
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
