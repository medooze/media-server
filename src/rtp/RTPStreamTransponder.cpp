/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPStreamTransponder.cpp
 * Author: Sergio
 * 
 * Created on 20 de abril de 2017, 18:33
 */

#include "rtp/RTPStreamTransponder.h"
#include "waitqueue.h"
#include "vp8/vp8.h"


RTPStreamTransponder::RTPStreamTransponder(RTPOutgoingSourceGroup* outgoing,RTPSender* sender) :
	mutex(true)
{
	//Store outgoing streams
	this->outgoing = outgoing;
	this->sender = sender;
	ssrc = outgoing->media.ssrc;
	
	//Add us as listeners
	outgoing->AddListener(this);
	
	Debug(">RTPStreamTransponder() | [outgoing:%p,sender:%p,ssrc:%u]\n",outgoing,sender,ssrc);
}


bool RTPStreamTransponder::SetIncoming(RTPIncomingSourceGroup* incoming, RTPReceiver* receiver)
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
                Reset();

                //Store stream and receiver
                this->incoming = incoming;
                this->receiver = receiver;
        }
	
	//Double check
	if (this->incoming)
	{
		//Add us as listeners
		incoming->AddListener(this);
		
		//Request update on the incoming
		if (this->receiver) this->receiver->SendPLI(this->incoming->media.ssrc);
	}
	
	Debug("<RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p]\n",incoming,receiver);
	
	//OK
	return true;
}

RTPStreamTransponder::~RTPStreamTransponder()
{
	//Clean all pending packets in queue
	Reset();
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


void RTPStreamTransponder::onRTP(RTPIncomingSourceGroup* group,const RTPPacket::shared& packet)
{
	//Double check
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
		Debug("-StreamTransponder::onRTP dropping empty packet\n");
		//Drop it
		dropped++;
		//Exit
		return;
	}
	
	ScopedLock lock(mutex);
	
	//Check sender
	if (!sender)
		//Nothing
		return;
	
	//Check if the source has changed
	if (source && packet->GetSSRC()!=source)
	{
		//IF last was not completed
		if (!lastCompleted && type==MediaFrame::Video)
		{
			//Create new RTP packet
			RTPPacket::shared rtp = std::make_shared<RTPPacket>(media,codec);
			//Set data
			rtp->SetPayloadType(type);
			rtp->SetSSRC(source);
			rtp->SetExtSeqNum(lastExtSeqNum++);
			rtp->SetMark(true);
			rtp->SetTimestamp(lastTimestamp);
			//Send it
			sender->Enqueue(rtp);
		}
		//Reset first paquet
		firstExtSeqNum = 0;
		//Not selecting
		if (selector)
		{
			//Delete and reset
			delete(selector);
			selector = nullptr;
		}
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
			
			//UltraDebug("-ts offset:%llu diff:%llu baseTimestap:%lu firstTimestamp:%lu lastTimestamp:%llu rate:%llu\n",offset,diff,baseTimestamp,firstTimestamp,lastTimestamp,packet->GetClockRate());
			
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
		firstTimestamp = packet->GetTimestamp();
		
		UltraDebug("-first seq:%lu base:%lu last:%lu ts:%lu baseSeq:%lu baseTimestamp:%llu lastTimestamp:%llu\n",firstExtSeqNum,baseExtSeqNum,lastExtSeqNum,firstTimestamp,baseExtSeqNum,baseTimestamp,lastTimestamp);
	}
	
	//Ensure it is not before first one
	if (extSeqNum<firstExtSeqNum)
		//Exit
		return;
	
	//Only for viedo
	if (packet->GetMedia()==MediaFrame::Video)
	{
		//Check if we don't have one or if we have a selector and it is not from the same codec
		if (!selector || (BYTE)selector->GetCodec()!=packet->GetCodec())
		{
			//If we had one
			if (selector)
				//Delete it and reset
				delete(selector);
			//Create new selector for codec
			selector = VideoLayerSelector::Create((VideoCodec::Type)packet->GetCodec());
			//Set prev layers
			selector->SelectSpatialLayer(spatialLayerId);
			selector->SelectTemporalLayer(temporalLayerId);
		}
	}
	
	bool mark = packet->GetMark();
	
	//Select layer
	if (selector && !selector->Select(packet,mark))
	{
		//One more dropperd
		dropped++;
		//Drop
		return;
	}
	
	//Clone packet
	auto cloned = packet->Clone();
	
	//Set normalized seq num
	extSeqNum = baseExtSeqNum + (extSeqNum - firstExtSeqNum) - dropped;
	
	//UltraDebug("-ext seq:%lu base:%lu first:%lu current:%lu dropped:%lu\n",extSeqNum,baseExtSeqNum,firstExtSeqNum,packet->GetExtSeqNum(),dropped);
	
	//Set new seq numbers
	cloned->SetExtSeqNum(extSeqNum);
	
	//Set normailized timestamp
	cloned->SetTimestamp(baseTimestamp + (packet->GetTimestamp()-firstTimestamp));
	
	//Set mark again
	cloned->SetMark(mark);
	
	//Change ssrc
	cloned->SetSSRC(ssrc);
	
	//Disable frame markings
	//TODO: we should change this so the header extension is recvonly and stripted when sending it instead
	cloned->DisableFrameMarkings();
	
	//Rewrite pict id
	//TODO: this should go into the layer selector??
	if (rewritePicId && cloned->GetCodec()==VideoCodec::VP8)
	{
		VP8PayloadDescriptor desc;

		//Parse VP8 payload description
		desc.Parse(cloned->GetMediaData(),cloned->GetMediaLength());
		
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
		
		//Rewrite picture id wrapping to 15 or 7 bits
		desc.pictureId = desc.pictureIdLength==2 ? picId%0x7FFF : picId%0x7F;
		//Rewrite tl0 index
		desc.temporalLevelZeroIndex = tl0Idx;
		
		//Write it back
		desc.Serialize(cloned->GetMediaData(),cloned->GetMediaLength());
	}
	//UPdate media codec and type
	media = cloned->GetMedia();
	codec = cloned->GetCodec();
	type  = cloned->GetPayloadType();
	//Get last send seq num and timestamp
	lastExtSeqNum = cloned->GetExtSeqNum();
	lastTimestamp = cloned->GetTimestamp();
	//Update last sent time
	lastTime = getTime();
	
	//Send packet
	sender->Enqueue(cloned);
}

void RTPStreamTransponder::onEnded(RTPIncomingSourceGroup* group)
{
	ScopedLock lock(mutex);
	
	//If they are the not same
	if (this->incoming!=group)
		//DO nothing
		return;
	
	//Reset packets before start listening again
	Reset();
	
	//Store stream and receiver
	this->incoming = nullptr;
	this->receiver = nullptr;
}

void RTPStreamTransponder::RequestPLI()
{
	ScopedLock lock(mutex);
	//Request update on the incoming
	if (receiver && incoming) receiver->SendPLI(incoming->media.ssrc);
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
	ScopedLock lock(mutex);
		
	this->spatialLayerId = spatialLayerId;
	this->temporalLayerId = temporalLayerId;
		
	if (!selector)
		return;
	
	if (selector->GetSpatialLayer()<spatialLayerId)
		//Request update on the incoming
		RequestPLI();
	selector->SelectSpatialLayer(spatialLayerId);
	selector->SelectTemporalLayer(temporalLayerId);
}

void  RTPStreamTransponder::Reset()
{
	ScopedLock lock(mutex);
		
	Debug(">RTPStreamTransponder::Reset()\n");
		
	//No source
	lastCompleted = true;
	source = 0;
	//Reset first paquet seq num and timestamp
	firstExtSeqNum = 0;
	firstTimestamp = 0;
	//Store the last send ones
	baseExtSeqNum = lastExtSeqNum+1;
	baseTimestamp = lastTimestamp;
	//None dropped
	dropped = 0;
	//Not selecting
	if (selector)
	{
		//Delete and reset
		delete(selector);
		selector = nullptr;
	}
	//No layer
	spatialLayerId = LayerInfo::MaxLayerId;
	temporalLayerId = LayerInfo::MaxLayerId;
	
	Debug("<RTPStreamTransponder::Reset()\n");
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
