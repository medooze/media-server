#ifndef _USE_H_
#define _USE_H_
#include <pthread.h>
#include "tools.h"

class Use
{
public:
	Use()
	{
		pthread_mutex_init(&mutex,NULL);
		pthread_mutex_init(&lock,NULL);
		pthread_cond_init(&cond,NULL);
		cont = 0;
	};

	~Use()
	{
		pthread_mutex_destroy(&mutex);
		pthread_mutex_destroy(&lock);
		pthread_cond_destroy(&cond);
	};

	void IncUse()
	{
		pthread_mutex_lock(&lock);
		pthread_mutex_lock(&mutex);
		cont ++;
		pthread_mutex_unlock(&mutex);
		pthread_mutex_unlock(&lock);
	}

	bool IncUse(DWORD timeout)
	{
		timespec ts;
		calcTimout(&ts,timeout);
		if (!pthread_mutex_timedlock(&lock,&ts))
			return false;
		pthread_mutex_lock(&mutex);
		cont ++;
		pthread_mutex_unlock(&mutex);
		pthread_mutex_unlock(&lock);
		return true;
	}

	void DecUse()
	{
		pthread_mutex_lock(&mutex);
		cont --;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	};

	void WaitUnusedAndLock()
	{
		pthread_mutex_lock(&lock);
		pthread_mutex_lock(&mutex);
		while(cont)
			pthread_cond_wait(&cond,&mutex);
	};

	bool WaitUnusedAndLock(DWORD timeout)
	{
		timespec ts;

		pthread_mutex_lock(&lock);
		pthread_mutex_lock(&mutex);
		//Calculate timeout
		calcTimout(&ts,timeout);
		while(cont)
		{
			if (pthread_cond_timedwait(&cond,&mutex,&ts))
			{
				pthread_mutex_unlock(&mutex);
				pthread_mutex_unlock(&lock);
				return false;
			}
		}
		return true;
	};

	void Unlock()
	{
		pthread_mutex_unlock(&mutex);
		pthread_mutex_unlock(&lock);
	};

private:
	pthread_mutex_t	mutex;
	pthread_mutex_t	lock;
	pthread_cond_t 	cond;
	int		cont;

};


class Mutex
{
public:
	Mutex()
	{
		pthread_mutex_init(&mutex,NULL);
	};

	Mutex(int recursive)
	{
		if (recursive)
		{
			//Crete recursive mutex
			pthread_mutexattr_t   mta;
			pthread_mutexattr_init(&mta);
			pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE_NP);
			pthread_mutex_init(&mutex, &mta);
		} else {
			pthread_mutex_init(&mutex,NULL);
		}
	};

	~Mutex()
	{
		pthread_mutex_destroy(&mutex);
	};

	void Lock()
	{
		pthread_mutex_lock(&mutex);
	};

	void Unlock()
	{
		pthread_mutex_unlock(&mutex);
	};

private:
	pthread_mutex_t	mutex;
};

class  ScopedLock
{
public:
	ScopedLock(Mutex &m) : mutex(m)
	{
		//Lock it
		mutex.Lock();
	}

	~ScopedLock()
	{
		//Unlock it
		mutex.Unlock();
	}
	
private:
	Mutex &mutex;
};
#endif
