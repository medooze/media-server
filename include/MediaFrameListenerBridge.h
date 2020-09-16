#ifndef MEDIAFRAMELISTENERBRIDGE_H
#define MEDIAFRAMELISTENERBRIDGE_H

#include "acumulator.h"
#include "media.h"
#include "rtp.h"
#include "EventLoop.h"

class MediaFrameListenerBridge :
	public MediaFrame::Listener,
	public RTPIncomingMediaStream
{
public:
	MediaFrameListenerBridge(DWORD ssrc);
	virtual ~MediaFrameListenerBridge();
	
        void AddMediaListener(MediaFrame::Listener *listener);	
	void RemoveMediaListener(MediaFrame::Listener *listener);
        
	virtual void AddListener(RTPIncomingMediaStream::Listener* listener);
	virtual void RemoveListener(RTPIncomingMediaStream::Listener* listener);
        
	virtual DWORD GetMediaSSRC() { return ssrc; }
	
	virtual void onMediaFrame(const MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame) { onMediaFrame(frame); }
	virtual TimeService& GetTimeService() { return loop; }
	void Reset();
	void Update();
	void Update(QWORD now);
        
        
public:
	EventLoop loop;
	DWORD ssrc = 0;
	DWORD extSeqNum = 0;
	Mutex mutex;
	std::set<RTPIncomingMediaStream::Listener*> listeners;
        std::set<MediaFrame::Listener*> mediaFrameListenerss;
	volatile bool reset	= false;
	QWORD firstTimestamp	= 0;
	QWORD baseTimestamp	= 0;
	QWORD lastTimestamp	= 0;
	QWORD lastTime		= 0;
	DWORD numFrames		= 0;
	DWORD numFramesDelta	= 0;
	DWORD numPackets	= 0;
	DWORD numPacketsDelta	= 0;
	DWORD totalBytes	= 0;
	DWORD bitrate		= 0;
	Acumulator acumulator;
	Acumulator accumulatorFrames;
	Acumulator accumulatorPackets;
	
	
	DWORD minWaitedTime	= 0;
	DWORD maxWaitedTime	= 0;
	long double avgWaitedTime = 0;
	Acumulator waited;
};

#endif /* MEDIAFRAMELISTENERBRIDGE_H */

