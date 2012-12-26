/* 
 * File:   rtpbuffer.h
 * Author: Sergio
 *
 * Created on 24 de diciembre de 2012, 10:27
 */

#ifndef RTPBUFFER_H
#define	RTPBUFFER_H

#include "rtp.h"

class RTPBuffer 
{
public:
	RTPBuffer()
	{
		//NO wait time
		maxWaitTime = 0;
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
			//Skip it and lost forever
			return true;

		//Add event
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
				if (next==(DWORD)-1 || seq==next || time+maxWaitTime<getTime())
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
				ts.tv_sec  = (time+maxWaitTime) / 1e6;
				ts.tv_nsec = (time+maxWaitTime) - ts.tv_sec*1e6;

				//Wait with time out
				int ret = pthread_cond_timedwait(&cond,&mutex,&ts);
				//Check if there is an errot different than timeout
				if (ret && ret!=ETIMEDOUT)
					//Print error
					Error("-WaitQueue cond timedwait error [%d,%d]\n",ret,errno);
			} else {
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
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	DWORD			next;
	DWORD			maxWaitTime;
};

#endif	/* RTPBUFFER_H */

