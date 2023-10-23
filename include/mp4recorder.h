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
#include "Buffer.h"
#include "EventLoop.h"

#include <deque>
#include <optional>

class mp4track
{
public:
	mp4track(MP4FileHandle mp4);
	int CreateAudioTrack(AudioCodec::Type codec, DWORD rate, bool disableHints = false);
	int CreateVideoTrack(VideoCodec::Type codec, DWORD rate, int width, int height, bool disableHints = false);
	int CreateTextTrack();
	int WriteAudioFrame(AudioFrame &audioFrame);
	int WriteVideoFrame(VideoFrame &videoFrame);
	int WriteTextFrame(TextFrame &textFrame);
	void SetTrackName(const std::string& name); 
	int Close();
	void AddH264SequenceParameterSet(const BYTE* data, DWORD size);
	void AddH264PictureParameterSet(const BYTE* data, DWORD size);
private:
	int FlushAudioFrame(AudioFrame* frame,DWORD duration);
	int FlushVideoFrame(VideoFrame* frame,DWORD duration);
	int FlushTextFrame(TextFrame* frame,DWORD duration);
private:
	MP4FileHandle mp4	= MP4_INVALID_FILE_HANDLE;
	MP4TrackId track	= 0;
	MP4TrackId hint		= 0;
	int sampleId		= 0;
	MediaFrame *frame	= nullptr;
	bool hasSPS		= false;
	bool hasPPS		= false;
	bool hasDimensions	= false;
	uint64_t firstSenderTime= 0;
	uint64_t firstTimestamp = 0;
	uint64_t lastSenderTime = 0;
	uint64_t lastTimestamp  = 0;
	uint32_t clockrate	= 0;
};


class MP4Recorder :
	public RecorderControl,
	public MediaFrame::Listener
{
public:
	class Listener 
	{
	public:
		virtual void onFirstFrame(QWORD time) = 0;
		virtual void onClosed() = 0;
	};
public:
	MP4Recorder(Listener* listener = nullptr);
	virtual ~MP4Recorder();

	//Recorder interface
	virtual bool Create(const char *filename);
	virtual bool Record();
	virtual bool Record(bool waitVideo);
		bool Record(bool waitVideo, bool disableHints);
	virtual bool Stop();
	virtual bool Close();
	bool Close(bool async);
	
	virtual RecorderControl::Type GetType()	{ return RecorderControl::MP4;	}

	virtual void onMediaFrame(const MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame);
	
	void SetTimeShiftDuration(DWORD duration) { timeShiftDuration = duration; }
	bool SetH264ParameterSets(const std::string& sprop);
	
private:
	void processMediaFrame(DWORD ssrc, const MediaFrame &frame, QWORD time);
private:	
	typedef std::unordered_map<DWORD, std::unique_ptr<mp4track>>	Tracks;
private:
	EventLoop	loop;
	Listener*	listener	= nullptr;
	MP4FileHandle	mp4		= MP4_INVALID_FILE_HANDLE;
	Tracks		audioTracks;
	Tracks		videoTracks;
	Tracks		textTracks;
	bool		recording	= false;
	bool		waitVideo	= false;
	bool		disableHints    = false;
	QWORD		first		= (QWORD)-1;
	
	std::deque<std::pair<uint32_t,std::unique_ptr<MediaFrame>>> timeShiftBuffer;
	std::optional<Buffer>	h264SPS;
	std::optional<Buffer>	h264PPS;
	DWORD timeShiftDuration = 0;
};
#endif
