#ifndef _AUDIO_H_
#define _AUDIO_H_
#include "config.h"
#include "media.h"
#include "codecs.h"

class AudioEncoder
{
public:
	// Must have virtual destructor to ensure child class's destructor is called
	virtual ~AudioEncoder(){};
	virtual int   Encode(SWORD *in,int inLen,BYTE* out,int outLen)=0;
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels)=0;
	virtual DWORD GetRate()=0;
	virtual DWORD GetNumChannels() { return 1; }
	virtual DWORD GetClockRate()=0;
	AudioCodec::Type	type;
	int			numFrameSamples;
};

class AudioDecoder
{
public:
	// Must have virtual destructor to ensure child class's destructor is called
	virtual ~AudioDecoder(){};
	virtual int   Decode(const BYTE *in,int inLen,SWORD* out,int outLen)=0;
	virtual DWORD TrySetRate(DWORD rate)=0;
	virtual DWORD GetRate()=0;
	virtual DWORD GetNumChannels() { return 1; }
	AudioCodec::Type	type;
	int			numFrameSamples;
};

class AudioFrame : public MediaFrame
{
public:
	AudioFrame(AudioCodec::Type codec) : MediaFrame(MediaFrame::Audio,2048)
	{
		//Store codec
		this->codec = codec;
	}
	
	AudioFrame(AudioCodec::Type codec,const std::shared_ptr<Buffer>& buffer) : MediaFrame(MediaFrame::Audio,buffer)
	{
		//Store codec
		this->codec = codec;
	}

	virtual MediaFrame* Clone() const
	{
		//Create new one with same data
		AudioFrame *frame = new AudioFrame(codec,buffer);
		//Set clock rate
		frame->SetClockRate(GetClockRate());
		//Set timestamp
		frame->SetTimestamp(GetTimeStamp());
		//Set time
		frame->SetTime(GetTime());
		frame->SetSenderTime(GetSenderTime());
		frame->SetTimestampSkew(GetTimestampSkew());
		//Set duration
		frame->SetDuration(GetDuration());
		//Set number of channels
		frame->SetNumChannels(GetNumChannels());
		//Set config
		if (HasCodecConfig()) frame->SetCodecConfig(GetCodecConfigData(),GetCodecConfigSize());
		//If we have disabled the shared buffer for this frame
		if (disableSharedBuffer)
			//Copy data
			frame->AdquireBuffer();
		//Check if it has rtp info
		for (const auto& rtp : rtpInfo)
			//Add it
			frame->AddRtpPacket(rtp.GetPos(),rtp.GetSize(),rtp.GetPrefixData(),rtp.GetPrefixLen());
		//Return it
		return (MediaFrame*)frame;
	}

	AudioCodec::Type GetCodec() const		{ return codec;		}
	void	SetCodec(AudioCodec::Type codec)	{ this->codec = codec;	}

	void SetNumChannels(int numChannels)		{ this->numChannels = numChannels;	}
	int  GetNumChannels() const			{ return numChannels;			}

	
private:
	AudioCodec::Type codec;
	int numChannels = 1;
};

class AudioInput
{
public:
	virtual DWORD GetNativeRate()=0;
	virtual DWORD GetRecordingRate()=0;
	virtual DWORD GetNumChannels()=0;
	virtual int RecBuffer(SWORD *buffer,DWORD size)=0;
	virtual int ClearBuffer() = 0;
	virtual void  CancelRecBuffer()=0;
	virtual int StartRecording(DWORD samplerate)=0;
	virtual int StopRecording()=0;

};

class AudioOutput
{
public:
	virtual DWORD GetNativeRate()=0;
	virtual DWORD GetPlayingRate()=0;
	virtual int PlayBuffer(SWORD *buffer,DWORD size,DWORD frameTime, BYTE vadLevel = -1) = 0;
	virtual int StartPlaying(DWORD samplerate, DWORD numChannels)=0;
	virtual int StopPlaying()=0;
};

#endif
