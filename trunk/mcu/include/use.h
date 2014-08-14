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
