#ifndef RTPWAITEDBUFFER_H
#define	RTPWAITEDBUFFER_H

#include <errno.h>
#include <pthread.h>
#include <map>

#include "config.h"
#include "acumulator.h"
#include "use.h"
#include "rtp/RTPPacket.h"

class RTPWaitedBuffer 
{
public:
	RTPWaitedBuffer() : waited(1000)
	{
		//Crete mutex
		pthread_mutex_init(&mutex,NULL);
		//Create condition
		pthread_cond_init(&cond,NULL);
	}

	virtual ~RTPWaitedBuffer()
	{
		//Free packets
		Clear();
		//Destroy mutex
		pthread_mutex_destroy(&mutex);
	}

	bool Add(const RTPPacket::shared& rtp)
	{
		//Get seq num
		DWORD seq = rtp->GetExtSeqNum();
		
		//Log
		//UltraDebug("-RTPWaitedBuffer::Add()  | rtp packet [next:%u,seq:%u,maxWaitTime=%d,cycles:%d-%u,time:%lld,current:%lld,hurry:%d]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum(),rtp->GetTime(),GetTime(),hurryUp);
		
		//Lock
		pthread_mutex_lock(&mutex);

		//If already past
		if (next!=(DWORD)-1 && seq<next)
		{
			//Error
			//UltraDebug("-RTPWaitedBuffer::Add() | Out of order non recoverable packet [next:%u,seq:%u,maxWaitTime=%d,cycles:%d-%u,time:%lld,current:%lld,hurry:%d]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum(),rtp->GetTime(),GetTime(),hurryUp);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Skip it and lost forever
			return 0;
		}
		
		//Check if we already have it
		if (packets.find(seq)!=packets.end())
		{
			//Error
			//UltraDebug("-RTPWaitedBuffer::Add() | Already have that packet [next:%u,seq:%u,maxWaitTime=%d,cycles:%d-%u]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum());
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Skip it and lost forever
			return 0;
		}

		//Add packet
		packets[seq] = rtp;
		
		//Unlock
		pthread_mutex_unlock(&mutex);

		//Signal
		pthread_cond_signal(&cond);

		return true;
	}

	void Cancel()
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//Canceled
		cancel = true;

		//Unlock
		pthread_mutex_unlock(&mutex);

		//Signal condition
		pthread_cond_signal(&cond);
	}
	
	RTPPacket::shared GetOrdered()
	{
		
		//Check if we have somethin in queue
		if (!packets.empty())
		{
			//Get first
			auto it = packets.begin();
			//Get first seq num
			DWORD seq = it->first;
			//Get packet
			auto candidate = it->second;
			//Get time of the packet
			QWORD time = candidate->GetTime();
			//Get now
			QWORD now = GetTime();

			//Check if first is the one expected or wait if not
			if (next==(DWORD)-1 || seq==next || time+maxWaitTime<=now || hurryUp)
			{
				//Update next
				next = seq+1;
				//Waiting time
				waited.Update(now,now-time);
				//Remove it
				packets.erase(it);
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
					return GetOrdered();
				}
				//Return it
				return candidate;
			}
		}
		//Rerturn 
		return NULL;
	}
	
	
	RTPPacket::shared Wait()
	{
		//NO packet
		RTPPacket::shared rtp;

		//Lock
		pthread_mutex_lock(&mutex);

		//While we have to wait
		while (!cancel)
		{
			//Check if we have something in queue
			if (!packets.empty())
			{
				//Get first
				auto it = packets.begin();
				//Get first seq num
				DWORD seq = it->first;
				//Get packet
				auto candidate = it->second;
				//Get time of the packet
				QWORD time = candidate->GetTime();
				//Get now
				QWORD now = GetTime();

				//Check if first is the one expected or wait if not
				if (next==(DWORD)-1 || seq==next || time+maxWaitTime<=now || hurryUp)
				{
					//Update next
					next = seq+1;
					//Waiting time
					waited.Update(now,now-time);
					//Remove it
					packets.erase(it);
					//If we have to skip it
					if (!candidate->GetMediaLength())
					{
						//Increase discarded packets count
						discarded++;
						//Try again
						continue;
					}
					//We have it!
					rtp = candidate;
					//Return it!
					break;
				}

				//We have to wait
				timespec ts;
				//Calculate until when we have to sleep
				ts.tv_sec  = (time+maxWaitTime) / 1000;
				ts.tv_nsec = (time+maxWaitTime - ts.tv_sec*1000)*1000;
				
				//Wait with time out
				int ret = pthread_cond_timedwait(&cond,&mutex,&ts);
				//Check if there is an errot different than timeout
				if (ret && ret!=ETIMEDOUT)
					//Print error
					Error("-WaitQueue cond timedwait error [%d,%d]\n",ret,errno);
				
			} else {
				//Not hurryUp more
				hurryUp = false;
				//Wait until we have a new rtp pacekt
				int ret = pthread_cond_wait(&cond,&mutex);
				//Check error
				if (ret)
					//Print error
					Error("-WaitQueue cond timedwait error [%d,%d]\n",ret,errno);
			}
		}
		
		//Unlock
		pthread_mutex_unlock(&mutex);

		//canceled
		return rtp;
	}

	void Clear()
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//And remove all from queue
		ClearPackets();

		//UnLock
		pthread_mutex_unlock(&mutex);
	}

	void HurryUp()
	{
		//Set flag
		hurryUp = true;
		//Signal condition and proccess rtp now
		pthread_cond_signal(&cond);
	}

	void Reset()
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//And remove all from queue
		ClearPackets();

		//None dropped
		discarded = 0;
		
		//And remove cancel
		cancel = false;

		//No next
		next = (DWORD)-1;

		//Signal condition
		pthread_cond_signal(&cond);
		
		//UnLock
		pthread_mutex_unlock(&mutex);
		
		//Clear stats
		waited.Reset(GetTime());
	}

	DWORD Length() const
	{
		//REturn objets in queu
		return packets.size();
	}

	DWORD GetMaxWaitTime() const
       	{
		return maxWaitTime;
	}

	
	void SetMaxWaitTime(DWORD maxWaitTime)
	{
		this->maxWaitTime = maxWaitTime;
	}
	
	// Set time in ms
	void SetTime(QWORD ms)
	{
		time = ms;
	}
	
	QWORD GetTime() const
	{
		if (!time)
			return getTime()/1000;
		return time;
	}
	
	DWORD GetMinWaitedime() const
	{
		//Lock
		pthread_mutex_lock(&mutex);
		//Get value
		DWORD minValueInWindow =  waited.GetMinValueInWindow();
		//Unlock
		pthread_mutex_unlock(&mutex);
		//return it
		return minValueInWindow;
	}
	
	DWORD GetMaxWaitedTime() const
	{
		//Lock
		pthread_mutex_lock(&mutex);
		//Get value
		DWORD maxValueInWindow =  waited.GetMaxValueInWindow();
		//Unlock
		pthread_mutex_unlock(&mutex);
		//return it
		return maxValueInWindow;
	}
	
	long double GetAvgWaitedTime() const
	{
		//Lock
		pthread_mutex_lock(&mutex);
		//Get value
		long double media =  waited.GetInstantMedia();
		//Unlock
		pthread_mutex_unlock(&mutex);
		//return it
		return media;
	}
	
	DWORD GetNumDiscardedPackets() const
	{
		return discarded;
	}
	
private:
	void ClearPackets()
	{
		//Clear all list
		packets.clear();
	}

private:
	//The event list
	std::map<DWORD,RTPPacket::shared> packets;
	mutable pthread_mutex_t	mutex;
	pthread_cond_t cond;
	MinMaxAcumulator<uint32_t, uint64_t> waited;
	
	bool  cancel		= false;
	bool  hurryUp		= false;
	DWORD next		= (DWORD)-1;
	DWORD maxWaitTime	= 0;
	QWORD time		= 0;
	DWORD discarded		= 0;
};

#endif	/* RTPBUFFER_H */

