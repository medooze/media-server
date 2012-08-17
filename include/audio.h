#ifndef _AUDIO_H_
#define _AUDIO_H_
#include "config.h"
#include "media.h"
#include "codecs.h"

class AudioFrame : public MediaFrame
{
public:
	AudioFrame(AudioCodec::Type codec) : MediaFrame(MediaFrame::Audio,512)
	{
		//Store codec
		this->codec = codec;
		//Init values
		duration = 0;
	}

	virtual MediaFrame* Clone()
	{
		//Create new one
		AudioFrame *frame = new AudioFrame(codec);
		//Copy content
		frame->SetMedia(buffer,length);
		//Duration
		frame->SetDuration(duration);
		//Set timestamp
		frame->SetTimestamp(GetTimeStamp());
		//Return it
		return (MediaFrame*)frame;
	}

	DWORD GetDuration()		{ return duration;		}
	AudioCodec::Type GetCodec()	{ return codec;			}
	
	void SetCodec(AudioCodec::Type codec)	{ this->codec = codec;		}
	void SetDuration(DWORD duration)	{ this->duration = duration;	}

private:
	DWORD	duration;
	AudioCodec::Type codec;
};

class AudioInput
{
public:
	virtual int RecBuffer(SWORD *buffer,DWORD size)=0;
	virtual void  CancelRecBuffer()=0;
	virtual int StartRecording()=0;
	virtual int StopRecording()=0;
};

class AudioOutput
{
public:
	virtual int PlayBuffer(SWORD *buffer,DWORD size,DWORD frameTime)=0;
	virtual int StartPlaying()=0;
	virtual int StopPlaying()=0;
};

#endif
