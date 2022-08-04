#ifndef OBJEC_POLL_H_
#define OBJEC_POLL_H_

#include "vector"

template <typename T>
class ObjectPool
{
private:
	constexpr static auto npos = std::numeric_limits<size_t>::max();
public:
	ObjectPool(std::size_t size) : queue(size)
	{
		head = 0;
		tail = size-1;
	}

	T pick()
	{
		
		// if buffer is empty, allocate new
		if (empty())
			return T();

		//Get item position
		auto pos = head;

		// move head foward
		head = (head + 1) % queue.size();

		//If it is last
		if (head == tail)
			//Empty
			head = npos;
		//Return it
		return std::move(queue[pos]);
	}

	void release(T&& t)
	{
		//If we don't have any more space
		if (full())
			//Do nothing
			return;

		//If it is first
		if (head == npos)
			//Initialize to current pos
			head = tail;

		//Reset object
		t.Reset();

		//Move item at back of buffer
		queue[tail] = std::move(t);

		//Update tail
		tail = (tail + 1) % queue.size();
	}

	void release(const std::vector<T>& v)
	{
		//For each item
		for (auto it = v.begin(); it!=v.end(); ++it)
			//Relaease it
			release(std::move(*it));
	}

	size_t length() const
	{
		if (empty()) return 0;
		return tail > head ? tail - head : queue.size() - head + tail;
	}

	size_t size() const
	{
		return queue.size();
	}

	bool empty() const
	{
		return head == npos;
	}

	bool full() const
	{
		return queue.size() == length();
	}

private:
	std::vector<T> queue;
	size_t head;
	size_t tail;
};

#endif //OBJEC_POLL_H_