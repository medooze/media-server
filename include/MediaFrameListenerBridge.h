#ifndef MEDIAFRAMELISTENERBRIDGE_H
#define MEDIAFRAMELISTENERBRIDGE_H

#include "acumulator.h"
#include "media.h"
#include "rtp.h"
#include "TimeService.h"

#include <queue>
#include <memory>
#include <mutex>
#include <numeric>

using namespace std::chrono_literals;

class MediaFrameListenerBridge :
	public MediaFrame::Listener,
	public MediaFrame::Producer,
	public RTPIncomingMediaStream,
	public RTPReceiver
{
public:
	using shared = std::shared_ptr<MediaFrameListenerBridge>;
	
	class TimeBase
	{
	public:
	
		virtual void Update(MediaFrame::Type type, std::chrono::milliseconds now, uint64_t ts);
		
		virtual int64_t GetState(std::chrono::milliseconds now, uint64_t ts, uint64_t& tsOut) const = 0;
		
		virtual void reset() = 0;
	};
	
	
	
	class TimeBaseImpl : public TimeBase
	{
	public:
	
		virtual void Update(MediaFrame::Type type, std::chrono::milliseconds now, uint64_t ts) override
		{		
			std::lock_guard<std::mutex> lock(mutex);
				
			if (!initialized)
			{
				if (refTime.count() != 0)
				{
					baseTs += (now.count() - refTime.count()) * 90000/1000;
				}
				
				refTime = now;
				refTimestamp = ts;	
				
				initialized = true;			
				return;
			}
			

			int64_t scheduledMs = (int64_t(ts) - int64_t(refTimestamp)) * 1000 / 90000;
			int64_t actualTimeMs = now.count() - refTime.count();
						
			int offset = actualTimeMs - scheduledMs;			
			if (offset > 10)  // Packet late
			{
				baseTs += ts - refTimestamp;
				
				refTime = now;
				refTimestamp = ts;
			}
			else if (offset < -200)  // Packet early
			{
				tsDiffMap[type] = offset;			
				auto otherType = type == MediaFrame::Audio ? MediaFrame::Video : MediaFrame::Audio;
				if (tsDiffMap[otherType] < -200)
				{
					// Make reference time earlier for same time stamp, which means actual time
					// becomes longer and packets will be flushed ealier.
					refTime -= std::chrono::milliseconds(20);					
					Log("Hurry up!\n");
				}
			}
		}
		
		
		virtual int64_t GetState(std::chrono::milliseconds now, uint64_t ts, uint64_t& tsOut) const override
		{
			std::lock_guard<std::mutex> lock(mutex);			
			
			auto diff = (int64_t(ts) - int64_t(refTimestamp)) * 1000 / 90000;
			auto diffMs = now.count() - refTime.count();
		
			tsOut = baseTs + int64_t(ts) - int64_t(refTimestamp);
		
			// If > 0, it means the packet needs to be dispatched.
			return diffMs - diff;
		}
		
		virtual void reset() override
		{
			std::lock_guard<std::mutex> lock(mutex);
					
			initialized = false;
		}
		
		
	private:
		
		bool initialized = false;
		
		std::chrono::milliseconds refTime {0};
		uint64_t refTimestamp = 0;
		
		uint64_t baseTs = 0;
		
		std::unordered_map<MediaFrame::Type, int64_t> tsDiffMap;
		
		mutable std::mutex mutex;
	};
	
	
	
public:
	static constexpr uint32_t NoSeqNum = std::numeric_limits<uint32_t>::max();
	static constexpr uint64_t NoTimestamp = std::numeric_limits<uint64_t>::max();
public:
	MediaFrameListenerBridge(TimeService& timeService, DWORD ssrc, bool smooth = false, TimeBase* timeBase = nullptr);
	
	virtual ~MediaFrameListenerBridge();

	void Reset();
	void Update();
	void Update(QWORD now);
	void UpdateAsync(std::function<void(std::chrono::milliseconds)> callback);
	void Stop();

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
	virtual TimeService& GetTimeService() override { return timeService; }
	virtual void Mute(bool muting) override;

	// RTPReceiver interface
	virtual int SendPLI(DWORD ssrc) override { return 1; };
	virtual int Reset(DWORD ssrc) override { return 1; };

private:
	void Dispatch(const std::vector<RTPPacket::shared>& packet);
        
public:
	TimeService& timeService;
	Timer::shared dispatchTimer;

	std::queue<std::pair<RTPPacket::shared, uint64_t>> packets;

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
	DWORD numFrames		= 0;
	DWORD numFramesDelta	= 0;
	DWORD numPackets	= 0;
	DWORD numPacketsDelta	= 0;
	DWORD totalBytes	= 0;
	DWORD bitrate		= 0;
	WORD  width		= 0;
	WORD  height		= 0;
	Acumulator<uint32_t, uint64_t> acumulator;
	Acumulator<uint32_t, uint64_t> accumulatorFrames;
	Acumulator<uint32_t, uint64_t> accumulatorPackets;

	
	std::chrono::milliseconds lastSent = 0ms;
	
	DWORD minWaitedTime	= 0;
	DWORD maxWaitedTime	= 0;
	long double avgWaitedTime = 0;
	MinMaxAcumulator<uint32_t, uint64_t> waited;
	volatile bool muted = false;
	
	TimeBase* timeBase = nullptr;
};

#endif /* MEDIAFRAMELISTENERBRIDGE_H */

