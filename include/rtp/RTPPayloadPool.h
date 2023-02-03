#ifndef RTPPAYLOAD_POOL_H_
#define RTPPAYLOAD_POOL_H_

#include "concurrentqueue.h"
#include "RTPPayload.h"

class RTPPayloadPool
{
public:
	RTPPayloadPool(std::size_t preallocate)
	{
		//Allocate some payload objects by default
		for (std::size_t i = 0; i < preallocate; ++i)
			pool.enqueue(new RTPPayload());
	}

	~RTPPayloadPool()
	{
		RTPPayload* payload;

		//Get all the object from the pool
		while (pool.try_dequeue(payload))
			//Delete them
			delete(payload);
	}

	RTPPayload::shared allocate()
	{
		RTPPayload* payload = nullptr;

		//Try to get one from the pool
		if (!pool.try_dequeue(payload))
			//Create a new one
			payload = new RTPPayload();

		//We need to create a new one
		return RTPPayload::shared(payload, [&](auto p) {
			//Reset it
			p->Reset();
			//Enqueue it back
			pool.enqueue(p);
		});
	}

private:
	moodycamel::ConcurrentQueue<RTPPayload*> pool;
};


#endif //RTPPAYLOAD_POOL_H