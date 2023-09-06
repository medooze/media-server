#ifndef _USE_H_
#define _USE_H_
#include <pthread.h>
#include "tools.h"
#include "log.h"
#include <map>

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

	void DecUse()
	{
		pthread_mutex_lock(&mutex);
		cont --;
		pthread_mutex_unlock(&mutex);
		pthread_cond_signal(&cond);
	};

	void WaitUnusedAndLock()
	{
		pthread_mutex_lock(&lock);
		pthread_mutex_lock(&mutex);
		while(cont)
			pthread_cond_wait(&cond,&mutex);
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

template<class Key, class T> class UseMap :
	public Use,
	public std::map<Key,T>
{
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
#ifdef PTHREAD_MUTEX_RECURSIVE
			pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
#else
			pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE_NP);
#endif
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

protected:
	pthread_mutex_t	mutex;
};


class WaitCondition :
	public Mutex
{
public:
	WaitCondition(bool recursive = false) : Mutex(recursive)
	{
		//No canceled
		cancel = false;
		//Create condition
		pthread_cond_init(&cond,NULL);
	}
	virtual ~WaitCondition()
	{
		//Destroy condition
		pthread_cond_destroy(&cond);
	}
	
	void Signal()
	{
		//Signal condition
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

		//if we are cancel
		if (cancel)
			//canceled
			return false;

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
			return false;

		//Not canceled
		return true;
	}
	
	void Reset()
	{
		//Lock
		pthread_mutex_lock(&mutex);

		//Not canceled
		cancel = false;

		//Unlock
		pthread_mutex_unlock(&mutex);
	}
protected:
	bool cancel;
	pthread_cond_t  cond;
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

class  ScopedUseLock
{
public:
	ScopedUseLock(Use &u) : use(u)
	{
		//Lock it
		use.WaitUnusedAndLock();
	}

	~ScopedUseLock()
	{
		//Unlock it
		use.Unlock();
	}
	
private:
	Use &use;
};

class  ScopedUse
{
public:
	ScopedUse(Use &u) : use(u)
	{
		//IncUse it
		use.IncUse();
	}

	~ScopedUse()
	{
		//DecUs it
		use.DecUse();
	}
	
private:
	Use &use;
};
#endif
