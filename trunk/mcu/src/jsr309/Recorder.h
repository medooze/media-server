/* 
 * File:   Recorder.h
 * Author: Sergio
 *
 * Created on 26 de febrero de 2012, 16:50
 */

#ifndef RECORDER_H
#define	RECORDER_H

#include "config.h"
#include <string>
#include "Joinable.h"
#include "mp4recorder.h"
#include <map>

class Recorder :
	public MP4Recorder,
	public Joinable::Listener
{
public:
	Recorder(std::wstring tag);
	virtual ~Recorder();

	//Joinable::Listener
	virtual void onRTPPacket(RTPPacket &packet);
	virtual void onResetStream();
	virtual void onEndStream();

	//Attach
	int Attach(MediaFrame::Type media, Joinable *join);
	int Dettach(MediaFrame::Type media);
private:
	typedef std::map<MediaFrame::Type,Joinable*> JoinedMap;
private:
	std::wstring tag;
	RTPDepacketizer* video;
	RTPDepacketizer* audio;
	JoinedMap	 joined;
};

#endif	/* RECORDER_H */

