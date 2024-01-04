#include "tracing.h"

#include "rtp/RTPStreamTransponder.h"
#include "waitqueue.h"
#include "vp8/vp8.h"
#include "av1/AV1LayerSelector.h"


RTPStreamTransponder::RTPStreamTransponder(const RTPOutgoingSourceGroup::shared& outgoing, const RTPSender::shared& sender) :
	timeService(outgoing->GetTimeService()),
	outgoing(outgoing),
	sender(sender)
{
	//Store outgoing streams
	ssrc = outgoing->media.ssrc;

	//Add us as listeners
	outgoing->AddListener(this);

	Debug("-RTPStreamTransponder() | [outgoing:%p,sender:%p,ssrc:%u]\n",outgoing,sender,ssrc);
}

void RTPStreamTransponder::ResetIncoming()
{
	SetIncoming(nullptr, nullptr);
}

void RTPStreamTransponder::SetIncoming(const RTPIncomingMediaStream::shared& incoming, const RTPReceiver::shared& receiver, bool smooth)
{
	timeService.Async([=](auto now){
		//Check we are not closed
		if (!outgoing)
		{
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
				//DO nothing
				return;

			//Remove listener from previues transitioning stream
			if (this->incomingNext)
				this->incomingNext->RemoveListener(this);

			//If they are the same as current ones
			if (this->incoming == incoming && this->receiver == receiver)
			{
				//And don't wait anymore
				incomingNext = nullptr;
				receiverNext = nullptr;
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
				//DO nothing
				return;

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
}

RTPStreamTransponder::~RTPStreamTransponder()
{
	Debug("~RTPStreamTransponder::~RTPStreamTransponder() [this:%p]\n", this);

	//If not already stopped
	if (outgoing || incoming)
		//Stop listeneing
		Close();
}

void RTPStreamTransponder::Close()
{
	Debug(">RTPStreamTransponder::Close() [this:%p]\n",this);

	//Stop listening
	if (outgoing) outgoing->RemoveListener(this);

	timeService.Sync([=](auto now) {
		//Stop listening
		if (incoming) incoming->RemoveListener(this);
		if (incomingNext) incomingNext->RemoveListener(this);
		//Remove sources
		incoming = nullptr;
		receiver = nullptr;
		incomingNext = nullptr;

		receiverNext = nullptr;
		//Remove sources
		outgoing = nullptr;
		sender = nullptr;
	});

	Debug("<RTPStreamTransponder::Close() [this:%p]\n",this);
}


void RTPStreamTransponder::onRTP(const RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Trace method
	TRACE_EVENT("rtp","RTPStreamTransponder::onRTP", "ssrc", packet->GetSSRC(), "seqnum", packet->GetSeqNum());

	if (!packet)
		//Exit
		return;

	//Run async
	timeService.Async([=, packet = packet->Clone()](auto now){

		//If it is from the next transitioning stream
		if (stream == incomingNext.get())
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
		if (stream != this->incoming.get())
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

			UltraDebug("-StreamTransponder::onRTP() | first seq:%u base:%u last:%u ts:%llu baseSeq:%u baseTimestamp:%llu lastTimestamp:%llu\n",firstExtSeqNum,baseExtSeqNum,lastExtSeqNum,firstTimestamp,baseExtSeqNum,baseTimestamp,lastTimestamp);
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

		//TODO: this should go into the layer selector??
		if (codec==VideoCodec::VP8 && packet->vp8PayloadDescriptor)
		{
			//Get VP8 description
			auto& vp8PayloadDescriptor = *packet->vp8PayloadDescriptor;

			//Check if we have a pictId
			if (vp8PayloadDescriptor.pictureIdPresent)
			{
				//If we have not received any yet
				if (!pictureId)
				{
					//Use current as starting point
					pictureId = vp8PayloadDescriptor.pictureId;
					
				} 
				//If picture id is different than last received one
				else if (vp8PayloadDescriptor.pictureId != lastSrcPictureId)
				{
					//Increase picture id
					(*pictureId)++;
				}

				//Update last received pict id
				lastSrcPictureId = vp8PayloadDescriptor.pictureId;
			}

			//Check if we have a new base layer
			if (vp8PayloadDescriptor.temporalLevelZeroIndexPresent)
			{
				/*
				 * TL0PICIDX:  8 bits temporal level zero index.TL0PICIDX is a
				 * running index for the temporal base layer frames, i.e., the
				 * frames with TID set to 0.  If TID is larger than 0, TL0PICIDX
				 * indicates on which base - layer frame the current image depends.
				 * TL0PICIDX MUST be incremented when TID is 0.  The index MAY
				 * start at a random value, and it MUST wrap to 0 after reaching
				 * the maximum number 255.  Use of TL0PICIDX depends on the
				 * presence of TID.Therefore, it is RECOMMENDED that the TID be
				 * used whenever TL0PICIDX is.
				*/

				//Check if it is the base temporal layer
				if (vp8PayloadDescriptor.temporalLayerIndex == 0)
				{
					// If we have not received any tl0 index yet
					if (!temporalLevelZeroIndex)
					{
						//Use current as starting point
						temporalLevelZeroIndex = vp8PayloadDescriptor.temporalLevelZeroIndex;
					}
					//If it is different than last received tl0 index
					else if (vp8PayloadDescriptor.temporalLevelZeroIndex != lastSrcTemporalLevelZeroIndex)
					{
						//Increase tl0 index
						(*temporalLevelZeroIndex)++;
					}

					//Update last received tl0 index
					lastSrcTemporalLevelZeroIndex = vp8PayloadDescriptor.temporalLevelZeroIndex;
				}

			}

			// Set if we need rewrite the picture ID or tl0 index
			if (temporalLevelZeroIndex && pictureId && 
				( vp8PayloadDescriptor.pictureId != *pictureId || vp8PayloadDescriptor.temporalLevelZeroIndex != *temporalLevelZeroIndex)
			)
			{
				//Rewrite ids
				packet->rewitePictureIds = true;

				//Rewrite picture id
				vp8PayloadDescriptor.pictureId = *pictureId;
				//Rewrite tl0 index
				vp8PayloadDescriptor.temporalLevelZeroIndex = *temporalLevelZeroIndex;
			}

			//Error("-ext seq:%lu pictureIdPresent:%d rewrite:%d pictId:%d lastPictId:%d origPictId:%d intra:%d mark:%d \n",extSeqNum, vp8PayloadDescriptor.pictureIdPresent, packet->rewitePictureIds, *pictureId, lastSrcPicId, vp8PayloadDescriptor.pictureId : -1, packet->IsKeyFrame(), packet->GetMark());
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
		if (codec==VideoCodec::AV1 && selector)
			//Get decode target
			forwaredDecodeTargets = static_cast<AV1LayerSelector*>(selector.get())->GetForwardedDecodeTargets();

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

		//Set new seq numbers
		packet->SetExtSeqNum(extSeqNum);
		//Set normailized timestamp
		packet->SetTimestamp(timestamp);
		//Set mark again
		packet->SetMark(mark);
		//Change ssrc
		packet->SetSSRC(ssrc);

		//If it has a dependency descriptor
		if (forwaredDecodeTargets && packet->HasTemplateDependencyStructure())
			//Override mak
			packet->OverrideActiveDecodeTargets(forwaredDecodeTargets);
		//If we have a continous frame number
		if (packet->HasDependencyDestriptor() && continousFrameNumber != NoFrameNum)
			//Update it
			packet->OverrideFrameNumber(static_cast<uint16_t>(continousFrameNumber));

		//Send packet
		if (sender)
			//Enqueue it
			sender->Enqueue(packet);
	});
}

void RTPStreamTransponder::onBye(const RTPIncomingMediaStream* stream)
{
	timeService.Async([=](auto) {

		//If they are the not same
		if (this->incoming.get() != stream)
			//DO nothing
			return;

		//Reset packets
		reset = true;
	});
}

void RTPStreamTransponder::onEnded(const RTPIncomingMediaStream* stream)
{
	timeService.Async([=](auto){
		//IF it is the current one
		if (this->incoming.get() == stream)
		{
			//Reset packets before start listening again
			reset = true;

			//No stream and receiver
			this->incoming = nullptr;
			this->receiver = nullptr;
		//If it is the next one
		} else if (this->incomingNext.get() == stream) {
			//No transitioning stream and receiver
			this->incomingNext = nullptr;
			this->receiverNext = nullptr;
		}
	});
}
void RTPStreamTransponder::onEnded(const RTPOutgoingSourceGroup* group)
{
	timeService.Async([=](auto) {
		//IF it is the current one
		if (this->outgoing.get() == group)
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

void RTPStreamTransponder::onPLIRequest(const RTPOutgoingSourceGroup* group,DWORD ssrc)
{
	//Log
	UltraDebug("-RTPStreamTransponder::onPLIRequest()\n");
	RequestPLI();
}

void RTPStreamTransponder::onREMB(const RTPOutgoingSourceGroup* group,DWORD ssrc, DWORD bitrate)
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
