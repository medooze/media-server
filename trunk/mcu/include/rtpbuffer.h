/* 
 * File:   rtpbuffer.h
 * Author: Sergio
 *
 * Created on 24 de diciembre de 2012, 10:27
 */

#ifndef RTPBUFFER_H
#define	RTPBUFFER_H
#include <errno.h>
#include <pthread.h>
#include "rtp.h"
#include "use.h"

class RTPBuffer 
{
public:
	RTPBuffer()
	{
		//NO wait time
		maxWaitTime = 0;
		//No hurring up
		hurryUp = false;
		//No canceled
		cancel = false;
		//No next
		next = (DWORD)-1;
		//Crete mutex
		pthread_mutex_init(&mutex,NULL);
		//Create condition
		pthread_cond_init(&cond,NULL);
	}

	virtual ~RTPBuffer()
	{
		//Free packets
		Clear();
		//Destroy mutex
		pthread_mutex_destroy(&mutex);
	}

	bool Add(RTPTimedPacket *rtp)
	{
		//Get seq num
		DWORD seq = rtp->GetExtSeqNum();
		
		//Lock
		pthread_mutex_lock(&mutex);

		//If already past
		if (next!=(DWORD)-1 && seq<next)
		{
			//Error
			Debug("-Out of order non recoverable packet [next:%d,seq:%d,maxWaitTime=%d,%d,%d]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum());
			//Delete pacekt
			delete(rtp);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Skip it and lost forever
			return 0;
		}
		
		//Check if we already have it
		if (packets.find(seq)!=packets.end())
		{
			//Error
			Debug("-Already have that packet [next:%d,seq:%d,maxWaitTime=%d,%d,%d]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum());
			//Delete pacekt
			delete(rtp);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Skip it and lost forever
			return 0;
		}

		//Add packet, check if it was already there
		if (!packets.insert(std::pair<DWORD,RTPTimedPacket*>(seq,rtp)).second)
		{
			//Error
			Debug("-Error inserting packet [next:%d,seq:%d,maxWaitTime=%d,%d,%d]\n",next,seq,maxWaitTime,rtp->GetSeqCycles(),rtp->GetSeqNum());
			//Delete pacekt
			delete(rtp);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Skip it and lost forever
			return 0;
		}

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

	RTPPacket* Wait()
	{
		//NO packet
		RTPTimedPacket* rtp = NULL;

		//Get default wait time
		DWORD timeout = maxWaitTime;

		//Lock
		pthread_mutex_lock(&mutex);

		//While we have to wait
		while (!cancel)
		{
			//Check if we have somethin in queue
			if (!packets.empty())
			{

				//Get first
				RTPOrderedPackets::iterator it = packets.begin();
				//Get first seq num
				DWORD seq = it->first;
				//Get packet
				RTPTimedPacket* candidate = it->second;
				//Get time of the packet
				QWORD time = candidate->GetTime();

				//Check if first is the one expected or wait if not
				if (next==(DWORD)-1 || seq==next || time+maxWaitTime<getTime()/1000 || hurryUp)
				{
					//We have it!
					rtp = candidate;
					//Update next
					next = seq+1;
					//Remove it
					packets.erase(it);
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
					Error("-WaitQueue cond timedwait error [%rd,%d]\n",ret,errno);
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

		//And remove cancel
		cancel = false;

		//No next
		next = (DWORD)-1;

		//Signal condition
		pthread_cond_signal(&cond);
		
		//UnLock
		pthread_mutex_unlock(&mutex);
	}

	DWORD Length()
	{
		//REturn objets in queu
		return packets.size();
	}
	void SetMaxWaitTime(DWORD maxWaitTime)
	{
		this->maxWaitTime = maxWaitTime;
	}
private:
	void ClearPackets()
	{
		//For each item, list shall be locked before
		for (RTPOrderedPackets::iterator it=packets.begin(); it!=packets.end(); ++it)
			//Delete rtp
			delete(it->second);
		//Clear all list
		packets.clear();
	}

private:
	typedef std::map<DWORD,RTPTimedPacket*> RTPOrderedPackets;

private:
	//The event list
	RTPOrderedPackets	packets;
	bool			cancel;
	bool			hurryUp;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	DWORD			next;
	DWORD			maxWaitTime;
};

#endif	/* RTPBUFFER_H */

