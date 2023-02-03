#ifndef VIDEOBUFFERPOOL_H_
#define VIDEOBUFFERPOOL_H_

#include "concurrentqueue.h"
#include "VideoBuffer.h"

class VideoBufferPool
{
public:
	VideoBufferPool(std::size_t preallocate, std::size_t maxallocate) :
		preallocate(preallocate),
		maxallocate(maxallocate)
	{
	}

	void SetSize(DWORD width, DWORD height)
	{
		//Make sure we have a new size
		if (this->width==width && this->height==height)
			//Do nothing
			return;

		//Try deallocate old buffers now, but we will check the size when allocating a new one
		VideoBuffer* buffer;

		//Get all the object from the pool
		while (pool.try_dequeue(buffer))
			//Delete them
			delete(buffer);

		//Store new size
		this->width = width;
		this->height = height;
		//Allocate some buffer objects by default
		for (std::size_t i = 0; i < preallocate; ++i)
			pool.enqueue(new VideoBuffer(width, height));
	}

	~VideoBufferPool()
	{
		VideoBuffer* buffer;

		//Get all the object from the pool
		while (pool.try_dequeue(buffer))
			//Delete them
			delete(buffer);
	}

	VideoBuffer::shared allocate()
	{
		VideoBuffer* buffer = nullptr;

		//Try to get one from the pool
		if (!pool.try_dequeue(buffer))
		{
			//Create a new one
			buffer = new VideoBuffer(width, height);
		}
		//Make sure that it is from same size
		else if (!buffer || buffer->GetWidth()!=width || buffer->GetHeight()!=height)
		{
			//Delete old one
			delete (buffer);
			//Create a new one
			buffer = new VideoBuffer(width, height);
		}


		//We need to create a new one
		return VideoBuffer::shared(buffer);
	}

private:
	std::size_t preallocate = 0;
	std::size_t maxallocate = 0;
	DWORD width = 0;
	DWORD height = 0;
	moodycamel::ConcurrentQueue<VideoBuffer*> pool;
};
#endif // !VIDEOBUFFERPOOL_H_
