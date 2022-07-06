#ifndef MEDIAFRAMELISTENERBRIDGE_H
#define MEDIAFRAMELISTENERBRIDGE_H

#include "acumulator.h"
#include "media.h"
#include "rtp.h"
#include "EventLoop.h"

#include <queue>

class MediaFrameListenerBridge :
	public MediaFrame::Listener,
	public RTPIncomingMediaStream
{
public:
	static constexpr uint32_t NoSeqNum = std::numeric_limits<uint32_t>::max();
	static constexpr uint64_t NoTimestamp = std::numeric_limits<uint64_t>::max();
public:
	MediaFrameListenerBridge(DWORD ssrc, bool smooth = false);
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

private:
	void Dispatch(const RTPPacket::shared& pacekt);
        
public:
	EventLoop loop;
	Timer::shared dispatchTimer;

	std::queue<std::pair<RTPPacket::shared,uint32_t>> packets;

	DWORD ssrc = 0;
	DWORD extSeqNum = 0;
	bool  smooth = true;
	Mutex mutex;
	std::set<RTPIncomingMediaStream::Listener*> listeners;
        std::set<MediaFrame::Listener*> mediaFrameListenerss;
	volatile bool reset	= false;
	QWORD firstTimestamp	= NoTimestamp;
	QWORD baseTimestamp	= 0;
	QWORD lastTimestamp	= 0;
	QWORD lastTime		= 0;
	DWORD numFrames		= 0;
	DWORD numFramesDelta	= 0;
	DWORD numPackets	= 0;
	DWORD numPacketsDelta	= 0;
	DWORD totalBytes	= 0;
	DWORD bitrate		= 0;
	Acumulator<uint32_t, uint64_t> acumulator;
	Acumulator<uint32_t, uint64_t> accumulatorFrames;
	Acumulator<uint32_t, uint64_t> accumulatorPackets;

	
	
	
	DWORD minWaitedTime	= 0;
	DWORD maxWaitedTime	= 0;
	long double avgWaitedTime = 0;
	Acumulator<uint32_t, uint64_t> waited;
};

#endif /* MEDIAFRAMELISTENERBRIDGE_H */

