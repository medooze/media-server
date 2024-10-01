#ifndef AUDIOBUFFERPOOL_H_
#define AUDIOBUFFERPOOL_H_

#include "concurrentqueue.h"
#include "AudioBuffer.h"

class AudioBufferPool
{
public:
	AudioBufferPool(std::size_t preallocate, std::size_t maxallocate) :
		preallocate(preallocate),
		maxallocate(maxallocate)
	{
	}

	void SetSize(DWORD numSamples, DWORD numChannels)
	{
		//Make sure we have a new size
		if (this->numSamples==numSamples && this->numChannels==numChannels)
			//Do nothing
			return;

		//Try deallocate old buffers now, but we will check the size when allocating a new one
		AudioBuffer* buffer;

		//Get all the object from the pool
		while (pool->try_dequeue(buffer))
			//Delete them
			delete(buffer);

		//Store new size
		this->numSamples = numSamples;
		this->numChannels = numChannels;
		//Allocate some buffer objects by default
		for (std::size_t i = 0; i < preallocate; ++i)
			pool->enqueue(new AudioBuffer(numSamples, numChannels));
	}

	~AudioBufferPool()
	{
		AudioBuffer* buffer;

		//Get all the object from the pool
		while (pool->try_dequeue(buffer))
			//Delete them
			delete(buffer);
	}

	AudioBuffer::shared Acquire()
	{
		AudioBuffer* buffer = nullptr;
		//Try to get one from the pool
		if (!pool->try_dequeue(buffer))
		{
			//Create a new one
			buffer = new AudioBuffer(numSamples, numChannels);
		}
		//Make sure that it is from same size
		else if (!buffer || buffer->GetNumSamples()!=numSamples || buffer->GetNumChannels()!=numChannels)
		{
			//Delete old one
			delete (buffer);
			//Create a new one
			buffer = new AudioBuffer(numSamples, numChannels);
		}
		//We need to create a new one
		return AudioBuffer::shared(buffer, [maxallocate = maxallocate, weak = std::weak_ptr<moodycamel::ConcurrentQueue<AudioBuffer*>>(pool) ](auto p) {
			//Try to get a reference to the pool, as it may have been already deleted
			auto pool = weak.lock();
			//Ensure we are not overallocating
			if (maxallocate && pool && pool->size_approx() < maxallocate)
			{
				// Enqueue it back
				pool->enqueue(p);
			} else {
				//Delete it
				delete (p);
			}
		});
	}

private:
	std::shared_ptr<moodycamel::ConcurrentQueue<AudioBuffer*>> pool = std::make_shared<moodycamel::ConcurrentQueue<AudioBuffer*>>();

	std::size_t preallocate = 0;
	std::size_t maxallocate = 0;

	DWORD numSamples = 0;
    DWORD numChannels = 0;
};
#endif // !AUDIOBUFFERPOOL_H_
