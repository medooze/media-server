/* 
 * File:   Player.h
 * Author: Sergio
 *
 * Created on 9 de septiembre de 2011, 0:11
 */

#ifndef PLAYER_H
#define	PLAYER_H

#include "RTPMultiplexer.h"
#include "mp4streamer.h"


class Player :
	public MP4Streamer,
	public MP4Streamer::Listener
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onEndOfFile(Player *player,void* param) = 0;
	};
public:
	Player(std::wstring tag);
	void SetListener(Player::Listener *listener,void* param);
	Joinable* GetJoinable(MediaFrame::Type media);

	/* MP4Streamer listener*/
	virtual void onRTPPacket(RTPPacket &packet);
	virtual void onTextFrame(TextFrame &text);
	virtual void onMediaFrame(MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, MediaFrame &frame);
	virtual void onEnd();

	std::wstring& GetTag() { return tag;	}
private:
	RTPMultiplexer audio;
	RTPMultiplexer video;
	RTPMultiplexer text;
	std::wstring tag;
	Player::Listener *listener;
	void* param;
};

#endif	/* PLAYER_H */

