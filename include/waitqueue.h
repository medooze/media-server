/* 
 * File:   waitqueue.h
 * Author: Sergio
 *
 * Created on 28 de septiembre de 2011, 0:20
 */

#ifndef WAITQUEUE_H
#define	WAITQUEUE_H
#include "config.h"
#include "use.h"
#include "tools.h"
#include "log.h"
#include <errno.h>
#include <list>


//TODO: refector or remove, it is kind of useless
template<typename T> 
class WaitQueue : public Use
{
public:
	WaitQueue<T>()
	{
		//No canceled
		cancel = false;
		//Crete mutex
		pthread_mutex_init(&mutex,NULL);
		//Create condition
		pthread_cond_init(&cond,NULL);
	}

	virtual ~WaitQueue<T>()
	{
		//Destroy mutex
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
	}

	void Add(T obj)
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//Add event
		events.push_back(obj);

		//Unlock
		pthread_mutex_unlock(&mutex);

		//Signal
		pthread_cond_signal(&cond);
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

	bool Wait(DWORD timeout)
	{
		int ret = 0;
		timespec ts;

		//Lock
		pthread_mutex_lock(&mutex);

		//if we are cancel
		if (cancel)
		{
			//Unlock
			pthread_mutex_unlock(&mutex);
			//canceled
			return false;
		}
		//Check if we have already somethin in queue
		if (!events.empty())
		{
			//Unlock
			pthread_mutex_unlock(&mutex);
			//canceled
			return true;
		}

		//Check if we have a time
		if (timeout)
		{
			//Calculate timeout
			calcTimout(&ts,timeout);

			//Wait with time out
			ret = pthread_cond_timedwait(&cond,&mutex,&ts);
			//Check if there is an errot different than timeout
			if (ret && ret!=ETIMEDOUT)
				//Print error
				Error("-WaitQueue cond timedwait error [%d,%d]\n",ret,errno);
		} else {
			//Wait with out timout
			ret=pthread_cond_wait(&cond,&mutex);
			//Check error
			if (ret)
				//Print error
				Error("-WaitQueue cond timedwait error [%d,%d]\n",ret,errno);
		}
		
		//If we have been cancel
		if (cancel)
			//Not ok
			ret = 1;

		//Unlock
		pthread_mutex_unlock(&mutex);

		//canceled
		return !ret;
	}

	T Peek()
	{
		T val = NULL;

		//Lock
		pthread_mutex_lock(&mutex);

		//Get event
		if (!events.empty())
			//Retreive firs
			val = events.front();

		//UnLock
		pthread_mutex_unlock(&mutex);

		return val;
	}

	T Pop()
	{
		T val = NULL;
		
		//Lock
		pthread_mutex_lock(&mutex);

		//Get event
		if (!events.empty())
		{
			//Retreive firs
			val = events.front();
			//And remove it from queue
			events.pop_front();
		}

		//UnLock
		pthread_mutex_unlock(&mutex);

		return val;
	}

	void Skip()
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//And remove it from queue
		events.pop_front();

		//UnLock
		pthread_mutex_unlock(&mutex);
	}

	void Clear()
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//And remove all from queue
		events.clear();

		//UnLock
		pthread_mutex_unlock(&mutex);
	}

	void Reset()
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//And remove all from queue
		events.clear();

		//And remove cancel
		cancel = false;
		
		//UnLock
		pthread_mutex_unlock(&mutex);
	}

	DWORD Length()
	{
		//REturn objets in queu
		return events.size();
	}

private:
	typedef std::list<T> ObjectList;

private:
	//The event list
	ObjectList	events;
	bool		cancel;
	pthread_mutex_t	mutex;
	pthread_cond_t  cond;
};

#endif	/* WAITQUEUE_H */

