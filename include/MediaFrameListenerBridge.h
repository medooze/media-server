#ifndef MEDIAFRAMELISTENERBRIDGE_H
#define MEDIAFRAMELISTENERBRIDGE_H

#include "acumulator.h"
#include "media.h"
#include "rtp.h"

class MediaFrameListenerBridge :
	public MediaFrame::Listener,
	public RTPIncomingMediaStream
{
public:
	MediaFrameListenerBridge(DWORD ssrc) : ssrc(ssrc), acumulator(1000) {}
	virtual ~MediaFrameListenerBridge();
	
        void AddMediaListener(MediaFrame::Listener *listener);	
	void RemoveMediaListener(MediaFrame::Listener *listener);
        
	virtual void AddListener(RTPIncomingMediaStream::Listener* listener);
	virtual void RemoveListener(RTPIncomingMediaStream::Listener* listener);
        
	virtual DWORD GetMediaSSRC() { return ssrc; }
	
	virtual void onMediaFrame(const MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame) { onMediaFrame(frame); }
	void Reset();
	void Update();
	void Update(QWORD now);
        
        
public:
	DWORD ssrc = 0;
	DWORD extSeqNum = 0;
	Mutex mutex;
	std::set<RTPIncomingMediaStream::Listener*> listeners;
        std::set<MediaFrame::Listener*> mediaFrameListenerss;
	volatile bool reset	= false;
	DWORD firstTimestamp	= 0;
	QWORD baseTimestamp	= 0;
	QWORD lastTimestamp	= 0;
	QWORD lastTime		= 0;
	DWORD numFrames		= 0;
	DWORD numPackets	= 0;
	DWORD totalBytes	= 0;
	DWORD bitrate		= 0;
	Acumulator acumulator;
};

#endif /* MEDIAFRAMELISTENERBRIDGE_H */

