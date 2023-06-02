#ifndef RTMPMP4STREAM_H
#define	RTMPMP4STREAM_H
#include "config.h"
#include "rtmp.h"
#include "rtmpstream.h"
#include "rtmpmessage.h"
#include "mp4streamer.h"
#include "audio.h"
#include <string>

class RTMPMP4Stream : 
	public RTMPNetStream,
	public MP4Streamer::Listener
{
public:
	RTMPMP4Stream(DWORD id,RTMPNetStream::Listener *listener);
	virtual ~RTMPMP4Stream();
	virtual void doPlay(QWORD transId, std::wstring& url,RTMPMediaStream::Listener* listener) override;
	virtual void doSeek(QWORD transId, DWORD time) override;
	virtual void doClose(QWORD transId, RTMPMediaStream::Listener* listener) override;

	/* MP4Streamer listener*/
	virtual void onRTPPacket(RTPPacket &packet);
	virtual void onTextFrame(TextFrame &text);
	virtual void onMediaFrame(MediaFrame &frame);
	virtual void onEnd();

private:
	MP4Streamer streamer;
	AVCDescriptor *desc;
	AudioDecoder *decoder;
	AudioEncoder *encoder;
};

#endif
