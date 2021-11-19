#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include <optional>
#include "WrapExtender.h"

template <typename T, typename S, uint16_t N>
class CircularBuffer
{

public:
	bool Set(S seq, const T& value)
	{
		//Calculate sequence wrapping
		extender.Extend(seq);

		//Get extended sequence number
		auto ext = extender.Recover(seq);

		//If no first
		if (first == std::numeric_limits<uint64_t>::max())
		{
			//Set it
			first = ext;
			last = ext;
		//Ensure that we don't go backwards
		} else if (ext<first) {
			
			//Drop
			return false;
		}

		//Calculate position
		auto current = GetPos(ext);

		//If it is the new max
		if (ext > last)
		{
			//Fill the empty spaces
			for (auto i = last+1; i<ext && i<last+N+1; ++i)
				//Empty it
				ringbuffer[GetPos(i)].reset();
			//Store last
			last = ext;
		}

		//Store item
		ringbuffer[current] = value;

		
		//Check if we need to move first
		if (GetLength()>N)
		{
			//Update first pos
			pos = (current + 1) % N;
			//Update first
			first = last - N + 1;
		}
		
		//Done
		return true;
	}

	bool IsPresent(S seq) const
	{
		//Get extended sequence number
		auto ext = extender.Recover(seq);

		//Ensure that we don't go backward or forward
		if (ext < first || ext > last)
			//Drop
			return false;

		//Calculate position
		auto current = GetPos(ext);

		//If we have it
		return ringbuffer[current].has_value();

	}

	const std::optional<T> Get(S seq) const
	{
		//Get extended sequence number
		auto ext = extender.Recover(seq);

		//Ensure that we don't go backward or forward
		if (ext < first || ext > last)
			//Drop
			return std::nullopt;

		//Calculate position
		auto current = GetPos(ext);

		//If we have it
		return ringbuffer[current];

	}

	S GetLastSeq() const
	{
		return static_cast<S>(last);
	}

	S GetFirstSeq() const
	{
		return static_cast<S>(first);
	}

	uint64_t GetLength() const
	{
		return first != std::numeric_limits<uint64_t>::max() ? last - first + 1 : 0;
	}
private:
	uint64_t GetPos(uint64_t seq) const
	{
		return  (pos  + (seq - first)) % N;
	}


private:
	std::array<std::optional<T>, N> ringbuffer = {};
	WrapExtender<S,uint64_t> extender;
	uint64_t first = std::numeric_limits<uint64_t>::max();
	uint64_t last = 0;
	uint64_t pos = 0;
};
#endif /* BITHISTORY_H */
