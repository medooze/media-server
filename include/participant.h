/* 
 * File:   participant.h
 * Author: Sergio
 *
 * Created on 19 de enero de 2012, 18:29
 */

#ifndef PARTICIPANT_H
#define	PARTICIPANT_H

#include "video.h"
#include "audio.h"
#include "text.h"
#include "rtpsession.h"

class Participant
{
public:
	enum Type { RTP=0,RTMP=1 };

public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onRequestFPU(Participant *part) = 0;
	};
public:
	Participant(Type type,int partId,const std::wstring &token) : token(token)
	{
		this->type = type;
		this->partId = partId;
	}

	virtual ~Participant()
	{
	}
	
	Type GetType()
	{
		return type;
	}
	
	void SetListener(Listener *listener)
	{
		//Store listener
		this->listener = listener;
	}

	DWORD GetPartId()
	{
		return partId;
	}

	const std::wstring& GetToken()
	{
		return token;
	}
	
	virtual int SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties &properties) = 0;
	virtual int SetAudioCodec(AudioCodec::Type codec,const Properties &properties) = 0;
	virtual int SetTextCodec(TextCodec::Type codec) = 0;

	virtual int SetVideoInput(VideoInput* input) = 0;
	virtual int SetVideoOutput(VideoOutput* output) = 0;
	virtual int SetAudioInput(AudioInput* input) = 0;
	virtual int SetAudioOutput(AudioOutput *output) = 0;
	virtual int SetTextInput(TextInput* input) = 0;
	virtual int SetTextOutput(TextOutput* output) = 0;

	virtual MediaStatistics GetStatistics(MediaFrame::Type media) = 0;
	virtual int SetMute(MediaFrame::Type media, bool isMuted) = 0;
	virtual int SendVideoFPU() = 0;

	virtual int Init() = 0;
	virtual int End() = 0;
	
	void SetName(const std::wstring& name)  { this->name.assign(name);	}
	std::wstring GetName()			{ return name;			}

protected:
	Type type;
	Listener *listener;
	DWORD partId;
	std::wstring token;
	std::wstring name;
};

#endif	/* PARTICIPANT_H */

