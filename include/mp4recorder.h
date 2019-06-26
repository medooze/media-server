#ifndef _MP4RECORDER_H_
#define _MP4RECORDER_H_
#include <mp4v2/mp4v2.h>
#include "config.h"
#include "codecs.h"
#include "audio.h"
#include "video.h"
#include "text.h"
#include "media.h"
#include "recordercontrol.h"


class mp4track
{
public:
	mp4track(MP4FileHandle mp4);
	int CreateAudioTrack(AudioCodec::Type codec, DWORD rate);
	int CreateVideoTrack(VideoCodec::Type codec, DWORD rate, int width, int height);
	int CreateTextTrack();
	int WriteAudioFrame(AudioFrame &audioFrame);
	int WriteVideoFrame(VideoFrame &videoFrame);
	int WriteTextFrame(TextFrame &textFrame);
	int Close();
private:
	int FlushAudioFrame(AudioFrame* frame,DWORD duration);
	int FlushVideoFrame(VideoFrame* frame,DWORD duration);
	int FlushTextFrame(TextFrame* frame,DWORD duration);
private:
	MP4FileHandle mp4;
	MP4TrackId track;
	MP4TrackId hint;
	bool first;
	int length;
	int sampleId;
	MediaFrame *frame;
	bool hasSPS;
	bool hasPPS;
};


class MP4Recorder :
	public RecorderControl,
	public MediaFrame::Listener
{
public:

	MP4Recorder();
	virtual ~MP4Recorder();

	//Recorder interface
	virtual bool Create(const char *filename);
	virtual bool Record();
	virtual bool Record(bool waitVideo);
	virtual bool Stop();
	virtual bool Close();
	bool Close(bool async);
	
	virtual RecorderControl::Type GetType()	{ return RecorderControl::MP4;	}

	virtual void onMediaFrame(const MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame, QWORD time);
private:
	typedef std::map<DWORD,mp4track*>	Tracks;
private:

	MP4FileHandle	mp4;
	Tracks		audioTracks;
	Tracks		videoTracks;
	Tracks		textTracks;
	bool		recording;
	int		waitVideo;
	pthread_mutex_t mutex;
	QWORD		first;
};
#endif
