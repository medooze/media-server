#ifndef _MP4STREAMER_H_
#define _MP4STREAMER_H_
#include <mp4v2/mp4v2.h>
#include "media.h"
#include "rtp.h"
#include "text.h"
#include "audio.h"
#include "video.h"
#include "codecs.h"
#include "avcdescriptor.h"

struct MP4RtpTrack
{
	class Listener : public MediaFrame::Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onRTPPacket(RTPPacket &packet) = 0;
	};

	MP4FileHandle mp4;
	MP4TrackId hint;
	MP4TrackId track;
	unsigned int timeScale;
	unsigned int sampleId;
	unsigned short numHintSamples;
	unsigned short packetIndex;
	unsigned int frameSamples;
	int frameSize;
	int frameTime;
	int frameType;
	MediaFrame::Type media;
	MediaFrame *frame;
	int codec;
	int type;
	RTPPacket rtp;

	MP4RtpTrack(MediaFrame::Type media,int codec,int type) : rtp(media,codec,type)
	{
		//Store values
		this->media = media;
		this->codec = codec;
		this->type = type;
		//Empty the rest
		mp4		= NULL;
		hint		= -1;
		track		= -1;
		timeScale	= 0;
		sampleId	= -1;
		numHintSamples	= 0;
		packetIndex	= -1;
		frame		= NULL;
		frameSamples	= 0;
		frameSize	= 0;
		frameType	= 0;
		frameTime	= 0;
		//Check media type
		switch (media)
		{
			case MediaFrame::Video:
				//Create video frame
				frame = new VideoFrame((VideoCodec::Type)codec,262143);
				break;
			case MediaFrame::Audio:
				//Create audio frame with 8Khz rate
				frame = new AudioFrame((AudioCodec::Type)codec,8000);
				break;
		}
	}
	~MP4RtpTrack()
	{
		//If media
		if (frame)
			//Delete it
			delete(frame);
	}
	int Reset();
	QWORD Read(Listener *listener);
	QWORD SeekNearestSyncFrame(QWORD time);
	QWORD SearchNearestSyncFrame(QWORD time);
	QWORD Seek(QWORD time);
	int SendH263SEI(Listener *listener);
	QWORD GetNextFrameTime();
};

struct MP4TextTrack
{
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onTextFrame(TextFrame &text) = 0;
	};

	MP4FileHandle mp4;
	MP4TrackId track;
	unsigned int timeScale;
	unsigned int sampleId;
	unsigned int frameSamples;
	int frameSize;
	int frameTime;
	int frameType;
	TextFrame frame;

	MP4TextTrack()
	{
		//Empty the rest
		mp4		= NULL;
		track		= -1;
		timeScale	= 0;
		sampleId	= -1;
		frameSamples	= 0;
		frameSize	= 0;
		frameType	= 0;
		frameTime	= 0;
	}
	int Reset();
	QWORD Read(Listener *listener);
	QWORD ReadPrevious(QWORD time,Listener *listener);
	QWORD Seek(QWORD time);
	QWORD GetNextFrameTime();
};

class MP4Streamer
{
public:
	class Listener : 
		public MP4RtpTrack::Listener,
		public MP4TextTrack::Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onEnd() = 0;
	};
public:
	MP4Streamer(Listener *listener);
	~MP4Streamer();

	int Open(const char* filename);
	bool HasAudioTrack()	{ return audio!=NULL;	}
	bool HasVideoTrack()	{ return video!=NULL;	}
	bool HasTextTrack()	{ return text!=NULL;	}
	DWORD GetAudioCodec()	{ return audio->codec;	}
	DWORD GetVideoCodec()	{ return video->codec;	}
	double GetDuration();
	DWORD GetVideoWidth();
	DWORD GetVideoHeight();
	DWORD GetVideoBitrate();
	double GetVideoFramerate();
	AVCDescriptor* GetAVCDescriptor();
	int Play();
	QWORD PreSeek(QWORD time);
	int Seek(QWORD time);
	QWORD Tell()		{ return t+seeked;	}
	int Stop();
	int Close();
	
protected:
	int PlayLoop();

private:
	static void* play(void *par);

private:
	Listener *listener;
	bool opened;
	bool playing;
	pthread_t 	thread;
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
	QWORD		seeked;
	QWORD		t;

	MP4FileHandle mp4;
	MP4RtpTrack *audio;
	MP4RtpTrack *video;
	MP4TextTrack *text;
};

#endif
