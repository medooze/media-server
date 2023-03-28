#ifndef RTPBUFFER_H
#define	RTPBUFFER_H

#include <map>

#include "config.h"
#include "acumulator.h"
#include "use.h"
#include "rtp/RTPPacket.h"
#include "TimeService.h"

class RTPBuffer 
{
public:
	RTPBuffer() : waited(1000) {}
	~RTPBuffer() = default;
	bool Add(const RTPPacket::shared& rtp)
	{
		//Get seq num
		DWORD seq = rtp->GetExtSeqNum();
		
		//If already past
		if (next!=(DWORD)-1 && seq<next)
		{
			//Error
			//UltraDebug("-RTPBuffer::Add() | Out of order non recoverable packet [next:%u,seq:%u,maxWaitTime=%d,cycles:%d-%u,time:%lld,current:%lld,hurry:%d]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum(),rtp->GetTime(),getTime(),hurryUp);
			//Skip it and lost forever
			return false;
		}
		
		//Check if we already have it
		if (packets.find(seq)!=packets.end())
		{
			//Error
			//UltraDebug("-RTPBuffer::Add() | Already have that packet [next:%u,seq:%u,maxWaitTime=%d,cycles:%d-%u]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum());
			//Skip it and lost forever
			return false;
		}

		//Add packet
		packets[seq] = rtp;
		
		return true;
	}
	
	RTPPacket::shared GetOrdered(QWORD now)
	{
		
		//Check if we have somethin in queue
		if (!packets.empty())
		{
			//Get first
			auto it = packets.begin();
			//Get first seq num
			DWORD seq = it->first;
			//Get time of the packet
			QWORD time = it->second->GetTime();

			//Check if first is the one expected or wait if not
			if (next==(DWORD)-1 || seq==next || time+maxWaitTime<=now || hurryUp)
			{
				//Get packet
				RTPPacket::shared candidate = std::move(it->second);
				//Remove it
				packets.erase(it);

				//Update next
				next = seq+1;
				//Waiting time
				waited.Update(now, now>time ? now-time : 0);
				
				//If no mor packets
				if (packets.empty())
					//Not hurryUp more
					hurryUp = false;
				//Skip if empty
				if (!candidate->GetMediaLength())
				{
					//This one is dropped
					discarded++;
					//Try next
					return GetOrdered(now);
				}
				//Return it
				return candidate;
			}
		}
		//Rerturn 
		return nullptr;
	}
	
	void Clear()
	{
		//Clear all list
		packets.clear();
	}

	void HurryUp()
	{
		//Set flag
		hurryUp = true;
	}

	void Reset()
	{
		//And remove all from queue
		Clear();

		//None dropped
		discarded = 0;
		
		//No next
		next = (DWORD)-1;

	}

	DWORD Length() const
	{
		//REturn objets in queu
		return packets.size();
	}
	
	void SetMaxWaitTime(DWORD maxWaitTime)
	{
		this->maxWaitTime = maxWaitTime;
	}

	DWORD GetMaxWaitTime() const
	{
		return maxWaitTime;
	}
	
	DWORD GetMinWaitedime() const
	{
		return waited.GetMinValueInWindow();
	}
	
	DWORD GetMaxWaitedTime() const
	{
		return waited.GetMaxValueInWindow();
	}
	

	std::pair<DWORD, DWORD> GetMinMaxWaitedTime() const
	{
		return waited.GetMinMaxValueInWindow();
	}

	long double GetAvgWaitedTime() const
	{
		//Get value
		long double media =  waited.GetInstantMedia();
		//return it
		return media;
	}
	
	DWORD GetNumDiscardedPackets() const
	{
		return discarded;
	}

	DWORD GetNextPacketSeqNumber() const
	{
		return next;
	}
	
	QWORD GetWaitTime(QWORD now)
	{
		//Check if we have somethin in queue
		if (packets.empty())
			//Forever
			return (QWORD)-1;
		
		//Get frist packet
		auto it = packets.begin();
		//Get first seq num
		DWORD seq = it->first;
		//Get packet
		auto candidate = it->second;
		//Get time of the packet
		QWORD time = candidate->GetTime();
		//Get wait time
		if (next==(DWORD)-1 || seq==next || time+maxWaitTime<=now || hurryUp)
			//Now!
			return 0;
		//Return wait time for next packet
		return time+maxWaitTime-now;
	}
	
private:
	//The event list
	std::map<DWORD,RTPPacket::shared> packets;
	MinMaxAcumulator<uint32_t, uint64_t> waited;
	
	bool  hurryUp		= false;
	DWORD next		= (DWORD)-1;
	DWORD maxWaitTime	= 0;
	DWORD discarded		= 0;
};

#endif	/* RTPBUFFER_H */

