#ifndef CIRCULARQUEUE_H
#define CIRCULARQUEUE_H

#include <vector>
#include <limits>
#include <cmath>
#include <stdint.h>

template <typename T>
class CircularQueue
{
private:
	constexpr static auto npos = std::numeric_limits<size_t>::max();
public:
	CircularQueue(std::size_t size = 0, bool autoGrow = true) : 
		autoGrow(autoGrow)
	{
		if (size) queue.reserve(size);
	}

	void push_back(const T& item)
	{
		//If we don't have any more space
		if (full())
		{
			//If we grow the queue as needed
			if (autoGrow)
				//Increase queue size
				grow(queue.capacity() ? std::ceil(queue.capacity() * 1.2) : 1);
			else 
				//Remove first
				pop_front();
		}

		//If it is first
		if (head == npos)
			//Initialize to current pos
			head = tail;

		//Insert item at back of buffer
		if (queue.size() <= tail)
			queue.insert(queue.begin() + tail, item);
		else
			queue[tail] = item;

		//Update tail
		tail = (tail + 1) % queue.capacity();
	}

	template<class... Args>
	void emplace_back(Args&&... args) 
	{
		//If we don't have any more space
		if (full())
		{ 
			//If we grow the queue as needed
			if (autoGrow)
				//Increase queue size
				grow(queue.capacity() ? std::ceil(queue.capacity() * 1.2) : 1);
			else
				//Remove first
				pop_front();
		}

		//If it is first
		if (head == npos)
			//Initialize to current pos
			head = tail;

		//Insert item at back of buffer
		if (queue.size() <= tail)
			queue.emplace(queue.begin() + tail, T(std::forward<Args>(args)...));
		else
			queue[tail] = T(std::forward<Args>(args)...);

		//Update tail
		tail = (tail + 1) % queue.capacity();
	}

	const T& front() const
	{
		return queue[head];
	}

	const T& back() const
	{
		return queue[tail > 0 ? tail - 1 : queue.capacity() - 1];
	}

	T& front()
	{
		return queue[head];
	}

	T& back()
	{
		return queue[tail > 0 ? tail - 1 : queue.capacity() - 1];
	}

	const T& at(size_t pos) const
	{
		return queue[(head + pos) % queue.capacity()];
	}

	bool pop_front()
	{
		// if buffer is empty, throw an error
		if (empty())
			return false;

		// move head foward
		head = (head + 1) % queue.capacity();

		//If it is last
		if (head == tail)
			//Empty
			head = npos;

		return true;
	}

	void grow(std::size_t size)
	{
		//Get current length and size
		auto len = length();
		auto oldSize = queue.capacity();

		//Do not shrink
		if (size <= oldSize)
			//Do nothing
			return;

		//Get how much do we increase size
		auto diff = size - queue.capacity();
		//Allocate current buffer
		queue.reserve(size);

		//If not empty
		if (!empty())
		{
			//If we have to move tail to the new allocated space
			if (tail <= head)
			{
				//Get iterators to the tail and previus queue end
				auto t = queue.begin() + tail;
				auto e = queue.begin() + oldSize;
				//Get iterator to the tail part that we need to move to the new allocated space
				auto m = queue.begin() + std::min(diff, tail);
				//Insert all that we can from the tail to the end
				queue.insert(e, queue.begin(), m);
				//If we have more on the tail than the new allocated space
				if (tail >= diff)
					//Move rest of tail to the beginning 
					std::copy(m, t, queue.begin());
			}
			//Update tail position
			tail = (head + len) % queue.capacity();
		}
		else {
			//Update positions
			head = npos;
			tail = 0;
		}

	}

	size_t length() const
	{
		if (empty()) return 0;
		return tail > head ? tail - head : queue.capacity() - head + tail;
	}

	size_t size() const
	{
		return queue.capacity();
	}

	bool empty() const
	{
		return head == npos;
	}

	bool full() const
	{
		return queue.capacity() == length();
	}

	void clear()
	{
		//Update positions
		head = npos;
		tail = 0;
	}

private:
	std::vector<T> queue;
	size_t head = npos;
	size_t tail = 0;
	bool autoGrow = true;
};
#endif /* CIRCULARQUEUE_H */
