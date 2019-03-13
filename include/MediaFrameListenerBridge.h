#ifndef MEDIAFRAMELISTENERBRIDGE_H
#define MEDIAFRAMELISTENERBRIDGE_H

#include "media.h"
#include "rtp.h"

class MediaFrameListenerBridge :
	public MediaFrame::Listener,
	public RTPIncomingMediaStream
{
public:
	MediaFrameListenerBridge(DWORD ssrc) : ssrc(ssrc) {}
	virtual ~MediaFrameListenerBridge() = default;
	
	virtual void AddListener(RTPIncomingMediaStream::Listener* listener);
	virtual void RemoveListener(RTPIncomingMediaStream::Listener* listener);
	virtual DWORD GetMediaSSRC() { return ssrc; }
	
	virtual void onMediaFrame(MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, MediaFrame &frame) { onMediaFrame(frame); }
	void Reset();
	
public:
	DWORD ssrc = 0;
	DWORD extSeqNum = 0;
	Mutex mutex;
	std::set<RTPIncomingMediaStream::Listener*> listeners;
	volatile bool reset	= false;
	DWORD firstTimestamp	= 0;
	QWORD baseTimestamp	= 0;
	QWORD lastTimestamp	= 0;
	QWORD lastTime		= 0;
};

#endif /* MEDIAFRAMELISTENERBRIDGE_H */

