#ifndef MEDIAFRAMELISTENERBRIDGE_H
#define MEDIAFRAMELISTENERBRIDGE_H

#include "acumulator.h"
#include "media.h"
#include "rtp.h"
#include "TimeService.h"
#include "TimestampChecker.h"
#include "FrameDispatchCoordinator.h"

#include <queue>
#include <memory>

using namespace std::chrono_literals;

class MediaFrameListenerBridge :
	public TimeServiceWrapper<MediaFrameListenerBridge>,
	public MediaFrame::Listener,
	public MediaFrame::Producer,
	public RTPIncomingMediaStream,
	public RTPReceiver
{
public:
	using shared = std::shared_ptr<MediaFrameListenerBridge>;
public:
	static constexpr uint32_t NoSeqNum = std::numeric_limits<uint32_t>::max();
	static constexpr uint64_t NoTimestamp = std::numeric_limits<uint64_t>::max();
	static constexpr int64_t  MaxDispatchingDelayMs = 5000;

	struct PacketScheduleInfo
	{
		PacketScheduleInfo(std::chrono::milliseconds scheduled, RTPPacket::shared packet, std::chrono::milliseconds duration) :
			scheduled(scheduled), packet(packet), duration(duration)
		{}
			
		std::chrono::milliseconds scheduled;
		RTPPacket::shared packet;
		std::chrono::milliseconds duration;
	};	
	
private:
	// Private constructor to prevent creating without TimeServiceWrapper::Create() factory
	friend class TimeServiceWrapper<MediaFrameListenerBridge>;
	MediaFrameListenerBridge(TimeService& timeService, DWORD ssrc, bool smooth = false, bool checkTimestamp = false);

public:
	virtual ~MediaFrameListenerBridge();

	virtual void OnCreated() override;
	
	void SetFrameDispatchCoordinator(const std::shared_ptr<FrameDispatchCoordinator>& coordinator);
	
	void Reset();
	void Update();
	void Update(QWORD now);
	void UpdateAsync(std::function<void(std::chrono::milliseconds)> callback);
	void Stop();
	
	void SetMaxDelayMs(std::chrono::milliseconds maxDelayMs);
	void SetDelayMs(std::chrono::milliseconds delayMs);
	void SetTargetBitrateHint(uint32_t targetBitrateHint);

	// MediaFrame::Producer interface
	virtual void AddMediaListener(const MediaFrame::Listener::shared& listener) override;
	virtual void RemoveMediaListener(const MediaFrame::Listener::shared& listener) override;

	// MediaFrame::Listener interface
	virtual void onMediaFrame(const MediaFrame& frame) override { onMediaFrame(0, frame); };
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame& frame) override;

	// RTPIncomingMediaStream interface
	virtual void AddListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual void RemoveListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual DWORD GetMediaSSRC() const override { return ssrc; }
	virtual TimeService& GetTimeService() override { return TimeServiceWrapper<MediaFrameListenerBridge>::GetTimeService(); }
	virtual void Mute(bool muting) override;

	// RTPReceiver interface
	virtual int SendPLI(DWORD ssrc) override { return 1; };
	virtual int Reset(DWORD ssrc) override { return 1; };

private:
	//To be run on timeservice thread
	void onMediaFrameAsync(std::chrono::milliseconds now, DWORD ignored, const std::shared_ptr<MediaFrame>& frame);
	void Dispatch(std::chrono::milliseconds now);
	void Update(std::chrono::milliseconds now);

	void DispatchPackets(const std::vector<RTPPacket::shared>& packet);

	
public:
	Timer::shared dispatchTimer;

	std::queue<PacketScheduleInfo> packets;	

	DWORD ssrc = 0;
	DWORD extSeqNum = 0;
	bool  smooth = true;
	std::set<RTPIncomingMediaStream::Listener*> listeners;
        std::set<MediaFrame::Listener::shared> mediaFrameListeners;
	volatile bool reset	= false;
	QWORD firstTimestamp	= NoTimestamp;
	QWORD baseTimestamp	= 0;
	QWORD lastTimestamp	= 0;
	QWORD lastTime		= 0;
	QWORD numFrames		= 0;
	QWORD numFramesDelta	= 0;
	QWORD numPackets	= 0;
	QWORD numPacketsDelta	= 0;
	QWORD totalBytes	= 0;
	DWORD bitrate		= 0;
	WORD  width		= 0;
	WORD  height		= 0;
	QWORD iframes		= 0;
	QWORD iframesDelta	= 0;
	QWORD bframes		= 0;
	QWORD bframesDelta	= 0;
	QWORD pframes		= 0;
	QWORD pframesDelta	= 0;

	MediaFrame::Type type = MediaFrame::Type::Unknown;
	BYTE codec = -1;

	Acumulator<uint32_t, uint64_t> acumulator;
	Acumulator<uint32_t, uint64_t> accumulatorFrames;
	Acumulator<uint32_t, uint64_t> accumulatorPackets;
	Acumulator<uint32_t, uint64_t> accumulatorIFrames;
	Acumulator<uint32_t, uint64_t> accumulatorBFrames;
	Acumulator<uint32_t, uint64_t> accumulatorPFrames;
	
	std::chrono::milliseconds lastSent = 0ms;
	
	DWORD minWaitedTime	= 0;
	DWORD maxWaitedTime	= 0;
	long double avgWaitedTime = 0;
	MinMaxAcumulator<uint32_t, uint64_t> waited;
	volatile bool muted = false;
	
	std::chrono::milliseconds dispatchingDelayMs = std::chrono::milliseconds(0);
	std::chrono::milliseconds maxDispatchingDelayMs = std::chrono::milliseconds(MaxDispatchingDelayMs);
		
	bool stopped = false;

	uint32_t targetBitrateHint = 0;
	
	std::unique_ptr<TimestampChecker> tsChecker;
	std::unique_ptr<TimestampChecker> ptsChecker;
	std::shared_ptr<FrameDispatchCoordinator> coordinator;
};

#endif /* MEDIAFRAMELISTENERBRIDGE_H */

