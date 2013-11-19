#ifndef _USE_H_
#define _USE_H_
#include <pthread.h>

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

#endif
