/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Participant.cpp
 * Author: Sergio
 * 
 * Created on 11 de enero de 2017, 14:56
 */

#include <set>

#include "SFUParticipant.h"
#include "rtp.h"


using namespace SFU;

Stream::Stream(Participant& p) : participant(p)
{
}

void Stream::RequestUpdate()
{
	//Debug
	Debug("-Stream::RequestUpdate() [id:%s,ssrc:%u]\n",msid.c_str(),video);
	//Send 
	participant.RequestUpdate();
}

Participant::Participant(DTLSICETransport* transport) : selector(VP9LayerSelector::MaxLayerId,VP9LayerSelector::MaxLayerId)
{
	this->transport = transport;
	//No audio or video
	audio = NULL;
	video = NULL;
}

Participant::~Participant()
{
	delete(transport);
	//Delete all groups
	for (auto it=groups.begin();it!=groups.end();++it)
		//Delete
		delete(it->second);
	if (audio)
		delete(audio);
	if (video)
		delete(video);
	if (mine)
		delete(mine);
}

void Participant::AddListener(Stream::Listener *other)
{
	//Lock method
	ScopedLock method(mutex);
	
	//add it as listener
	listeners.insert(other);
}

void Participant::RemoveListener(Stream::Listener *other)
{
	//Lock method
	ScopedLock method(mutex);
	
	//add it as listener
	listeners.erase(other);
}

bool Participant::AddLocalStream(Stream* stream)
{
	//Lock method
	ScopedLock method(mutex);
	
	Log("-Participant::AddLocalStream\n");
	
	//Add it to the stream from the others
	others[stream->msid] = stream;
	
	//If it has audio
	if (stream->audio)
	{
		//Create audio stream
		RTPOutgoingSourceGroup *oa = new RTPOutgoingSourceGroup(stream->msid,MediaFrame::Audio);
		//Set audio properties
		oa->media.ssrc = stream->audio;
		oa->fec.ssrc = 0;
		oa->rtx.ssrc = 0;
		//Add listener
		oa->AddListener(this);
		//Store group
		groups[oa->media.ssrc] = oa;
		//Add to transport
		transport->AddOutgoingSourceGroup(oa);
	}
	
	//If it has video
	if (stream->video)
	{
		//Create video stream
		RTPOutgoingSourceGroup *ov = new RTPOutgoingSourceGroup(stream->msid,MediaFrame::Video);
		//Set audio properties
		ov->media.ssrc = stream->video;
		ov->fec.ssrc = stream->fec;
		ov->rtx.ssrc = stream->rtx;
		//Store group
		groups[ov->media.ssrc] = ov;
		//Add listener
		ov->AddListener(this);
		//Add to transport
		transport->AddOutgoingSourceGroup(ov);
		
		//Request update for video
		stream->RequestUpdate();
	}

	
	return true;
}

bool Participant::RemoveLocalStream(Stream* stream)
{
	//Lock method
	ScopedLock method(mutex);
	
	Log("-Participant::RemoveLocalStream\n");
	
	//Remove local stream
	others.erase(stream->msid);
	
	//If it has audio
	if (stream->audio)
	{
		//Get audio group
		RTPOutgoingSourceGroup *oa = groups[stream->audio];
		//REmove it from transport
		transport->RemoveOutgoingSourceGroup(oa);
		//Delete it
		delete (oa);
	}
	
	//If it has video
	if (stream->video)
	{
		//Get audio group
		RTPOutgoingSourceGroup *ov = groups[stream->video];
		//REmove it from transport
		transport->RemoveOutgoingSourceGroup(ov);
		//Delete it
		delete (ov);
	}
	
	//OK
	return true;
}

bool Participant::AddRemoteStream(Properties &properties)
{
	//Lock method
	ScopedLock method(mutex);
	
	Log("-Participant::AddRemoteStream\n");
	
	//Get remote end stream properties
	Properties remote;
	properties.GetChildren("remote",remote);
	//Get local end stream properties
	Properties local;
	properties.GetChildren("local",local);
	
	//Get msid
	msid = remote.GetProperty("id");
	
	//If remote has audio
	if (remote.HasProperty("audio.ssrc"))
	{
		//Create audio stream
		audio = new RTPIncomingSourceGroup(MediaFrame::Audio);
		//Set audio properties
		audio->media.ssrc = remote.GetProperty("audio.ssrc",0);
		audio->fec.ssrc = remote.GetProperty("audio.fec.ssrc",0);
		audio->rtx.ssrc = remote.GetProperty("audio.rtx.ssrc",0);
		//Add listner
		audio->AddListener(this);

		//Add to transport
		transport->AddIncomingSourceGroup(audio);
	}
	
	//If remote has video
	if (remote.HasProperty("video.ssrc"))
	{
		//Create video stream
		video = new RTPIncomingSourceGroup(MediaFrame::Video);
		//Set video properties
		video->media.ssrc = remote.GetProperty("video.ssrc",0);
		video->fec.ssrc = remote.GetProperty("video.fec.ssrc",0);
		video->rtx.ssrc = remote.GetProperty("video.rtx.ssrc",0);
		//Add listner
		video->AddListener(this);
		//Add to transport
		transport->AddIncomingSourceGroup(video);
	}
	
	//Create stream
	mine = new Stream(*this);
	
	//Set data
	mine->msid = local.GetProperty("id");
	mine->audio = local.GetProperty("audio.ssrc",0);
	mine->video = local.GetProperty("video.ssrc",0);
	mine->fec = local.GetProperty("video.fec.ssrc",0);
	mine->rtx = local.GetProperty("video.rtx.ssrc",0);
	
	//Done
	return true;
}

void Participant::onRTP(RTPIncomingSourceGroup* group,RTPPacket* packet)
{	
	//Lock method
	ScopedLock method(mutex);
	
	if (group->type==MediaFrame::Audio)
	{
		//Change ssrc for outgoin stream
		packet->SetSSRC(mine->audio);
	} else {
		 DWORD extSeqNum;
		 bool mark;
		 //Select layer
		if (!selector.Select(packet,extSeqNum,mark))
			//Drop
			return;
		//Set them
		packet->SetSeqNum(extSeqNum);
		packet->SetSeqCycles(extSeqNum >> 16);
		//Set mark
		packet->SetMark(mark);
		//Change ssrc for outgoin stream
		packet->SetSSRC(mine->video);
	 }
		
	//For each listener
	for (auto it = listeners.begin();it!=listeners.end();++it)
		//send packet
		(*it)->onMedia(mine,packet);
		
}

void Participant::onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc)
{
	Debug("-Participant::onPLIRequest()\n");
	//Lock method
	ScopedLock method(mutex);
	
	//Should not happen
	if (group->type==MediaFrame::Audio)
		return;
	
	//Get stream 
	auto it = others.find(group->streamId);
	
	//Check
	if (it==others.end())
		return (void)Error("-No stream found for [%s]\n",group->streamId);
	//Get stream
	Stream* stream = it->second;
	//Call 
	stream->RequestUpdate();
}


 void Participant::onMedia(Stream* stream,RTPPacket* packet)
 {
	 //Send it
	 transport->Send(*packet);
 }
 
 void Participant::RequestUpdate() 
 {
	 //Check
	 if (video)
		//Send PLI on video media stream
		transport->SendPLI(video->media.ssrc);
 }
