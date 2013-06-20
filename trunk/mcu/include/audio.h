#ifndef _AUDIO_H_
#define _AUDIO_H_
#include "config.h"
#include "media.h"
#include "codecs.h"

class AudioEncoder
{
public:
	virtual int   Encode(SWORD *in,int inLen,BYTE* out,int outLen)=0;
	virtual DWORD TrySetRate(DWORD rate)=0;
	virtual DWORD GetRate()=0;
	virtual DWORD GetClockRate()=0;
	AudioCodec::Type	type;
	int			numFrameSamples;
	int			frameLength;
};

class AudioDecoder
{
public:
	virtual int   Decode(BYTE *in,int inLen,SWORD* out,int outLen)=0;
	virtual DWORD TrySetRate(DWORD rate)=0;
	virtual DWORD GetRate()=0;
	AudioCodec::Type	type;
	int			numFrameSamples;
	int			frameLength;
};

class AudioFrame : public MediaFrame
{
public:
	AudioFrame(AudioCodec::Type codec) : MediaFrame(MediaFrame::Audio,512)
	{
		//Store codec
		this->codec = codec;
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

	AudioCodec::Type GetCodec()	{ return codec;			}
	void SetCodec(AudioCodec::Type codec)	{ this->codec = codec;		}

private:
	AudioCodec::Type codec;
};

class AudioInput
{
public:
	virtual DWORD GetNativeRate()=0;
	virtual DWORD GetRecordingRate()=0;
	virtual int RecBuffer(SWORD *buffer,DWORD size)=0;
	virtual void  CancelRecBuffer()=0;
	virtual int StartRecording(DWORD samplerate)=0;
	virtual int StopRecording()=0;
};

class AudioOutput
{
public:
	virtual DWORD GetNativeRate()=0;
	virtual DWORD GetPlayingRate()=0;
	virtual int PlayBuffer(SWORD *buffer,DWORD size,DWORD frameTime)=0;
	virtual int StartPlaying(DWORD samplerate)=0;
	virtual int StopPlaying()=0;
};

class AudioCodecFactory
{
public:
	static AudioDecoder* CreateDecoder(AudioCodec::Type codec);
	static AudioEncoder* CreateEncoder(AudioCodec::Type codec);
	static AudioEncoder* CreateEncoder(AudioCodec::Type codec, const Properties &properties);
};

#endif
