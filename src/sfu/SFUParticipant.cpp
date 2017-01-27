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

Participant::Participant(DTLSICETransport* transport)
{
	this->transport = transport;
}

Participant::~Participant()
{
	delete(transport);
	if (audio)
		delete(audio);
	if (video)
		delete(video);
	if (mine)
		delete(mine);
}

void Participant::AddListener(Stream::Listener *other)
{
	//Add his outgoing stream to transport
	//transport->
	//add it as listener
	listeners.insert(other);
}

void Participant::RemoveListener(Stream::Listener *other)
{
	//Add his outgoing stream to transport
	//transport->
	//add it as listener
	listeners.erase(other);
}

bool Participant::AddLocalStream(Stream* stream)
{
	Log("-Participant::AddLocalStream\n");
	
	//Add it to the stream from the others
	others[stream->msid] = stream;
	
	//Create audio stream
	RTPOutgoingSourceGroup *oa = new RTPOutgoingSourceGroup(stream->msid,MediaFrame::Audio,this);
	//Set audio properties
	oa->media.SSRC = stream->audio;
	oa->fec.SSRC = 0;
	oa->rtx.SSRC = 0;
	
	//Add to transport
	transport->AddOutgoingSourceGroup(oa);
	
	//Create video stream
	RTPOutgoingSourceGroup *ov = new RTPOutgoingSourceGroup(stream->msid,MediaFrame::Video,this);
	//Set audio properties
	ov->media.SSRC = stream->video;
	ov->fec.SSRC = stream->fec;
	ov->rtx.SSRC = stream->rtx;
	
	//Add to transport
	transport->AddOutgoingSourceGroup(ov);
	
	//Request update
	stream->RequestUpdate();
	
	return true;
}

bool Participant::AddRemoteStream(Properties &properties)
{
	Log("-Participant::AddRemoteStream\n");
	
	//Get remote end stream properties
	Properties remote;
	properties.GetChildren("remote",remote);
	//Get local end stream properties
	Properties local;
	properties.GetChildren("local",local);
	
	//Get msid
	msid = remote.GetProperty("id");
		
	//Create audio stream
	audio = new RTPIncomingSourceGroup(MediaFrame::Audio,this);
	//Set audio properties
	audio->media.SSRC = remote.GetProperty("audio.ssrc",0);
	audio->fec.SSRC = remote.GetProperty("audio.fec.ssrc",0);
	audio->rtx.SSRC = remote.GetProperty("audio.rtx.ssrc",0);
	
	//Add to transport
	transport->AddIncomingSourceGroup(audio);
	
	//Create video stream
	video = new RTPIncomingSourceGroup(MediaFrame::Video,this);
	//Set audio properties
	video->media.SSRC = remote.GetProperty("video.ssrc",0);
	video->fec.SSRC = remote.GetProperty("video.fec.ssrc",0);
	video->rtx.SSRC = remote.GetProperty("video.rtx.ssrc",0);
	
	//Add to transport
	transport->AddIncomingSourceGroup(video);
	
	//Create stream
	mine = new Stream(*this);
	
	//Set data
	mine->msid = local.GetProperty("id");
	mine->audio = local.GetProperty("audio.ssrc",0);
	mine->video = local.GetProperty("video.ssrc",0);
	mine->fec = local.GetProperty("fec.ssrc",0);
	mine->rtx = local.GetProperty("rtx.ssrc",0);
	
	//Done
	return true;
}

void Participant::onRTP(RTPIncomingSourceGroup* group,RTPTimedPacket* packet)
{
	//packet->Dump();
	
	if (group->type==MediaFrame::Audio)
		//Change ssrc for outgoin stream
		packet->SetSSRC(mine->audio);
	else
		//Change ssrc for outgoin stream
		packet->SetSSRC(mine->video);
		
	//For each listener
	for (auto it = listeners.begin();it!=listeners.end();++it)
		//send packet
		(*it)->onMedia(mine,packet);
		
		
	delete(packet);
}

void Participant::onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc)
{
	Debug("-Participant::onPLIRequest() | [id:%s,ssrc:%u]\n",msid.c_str(),ssrc);
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


 void Participant::onMedia(Stream* stream,RTPTimedPacket* packet)
 {
	 //Debug("-Participant::onMedia\n");
	 //Send it
	 transport->Send(*packet);
 }
 
 void Participant::RequestUpdate() 
 {
	 Debug("-Participant:RequestUpdate()\n");
	//Send PLI on video media stream
	 transport->SendPLI(video->media.SSRC);
 }
