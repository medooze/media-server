/* 
 * File:   Player.cpp
 * Author: Sergio
 * 
 * Created on 9 de septiembre de 2011, 0:11
 */
#include "log.h"
#include "Player.h"
#include "mp4streamer.h"

Player::Player(std::wstring tag) : MP4Streamer(this)
{
	//Store tag
	this->tag = tag;
}

void Player::SetListener(Player::Listener *listener,void* param)
{
	//Store values
	this->listener = listener;
	this->param = param;
}

Joinable* Player::GetJoinable(MediaFrame::Type media)
{
	switch(media)
	{
		case MediaFrame::Video:
			//Return multiplexer
			return &video;
		case MediaFrame::Audio:
			//Return multiplexer
			return &audio;
		case MediaFrame::Text:
			//Return multiplexer
			return &text;
	}
}
	
void Player::onRTPPacket(RTPPacket &packet)
{
	switch(packet.GetMedia())
	{
		case MediaFrame::Video:
			//Multiplex
			video.Multiplex(packet);
			break;
		case MediaFrame::Audio:
			//Multiplex
			audio.Multiplex(packet);
			break;
	}
}

void Player::onTextFrame(TextFrame &frame)
{
	RTPPacket packet(MediaFrame::Text,TextCodec::T140,TextCodec::T140);
	//Set timestamp
	packet.SetTimestamp(frame.GetTimeStamp());
	//Copy
	packet.SetPayload(frame.GetData(),frame.GetLength());
	//Multiplex
	text.Multiplex(packet);
}

void Player::onEnd()
{
	//Reset audio stream
	audio.ResetStream();
	//Reset video stream
	video.ResetStream();
	//Reset text stream
	text.ResetStream();
	//Check for listener
	if (listener)
		//Send event
		listener->onEndOfFile(this,param);
}

void Player::onMediaFrame(MediaFrame &frame)
{
	
}

void Player::onMediaFrame(DWORD ssrc, MediaFrame &frame)
{
	
}

