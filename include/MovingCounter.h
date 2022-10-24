#ifndef MOVING_COUNTER_
#define MOVING_COUNTER_

#include <queue>
#include <optional>

// Implements moving max: can add samples to it and calculate maximum over some
// fixed moving window.
//
// Window size is configured at constructor.
// Samples can be added with |Add()| and max over current window is returned by
// |MovingMax|. |when| in successive calls to Add and MovingMax
// should never decrease as if it's a wallclock time.
template <typename  T>
class MovingMaxCounter {
public:
	explicit MovingMaxCounter(uint64_t window)
		: window(window) 
	{
	}

	// Advances the current time, and adds a new sample. The new current time must
	// be at least as large as the old current time.
	T Add(uint64_t when, const T& sample)
	{
		RollWindow(when);
		// Remove samples that will never be maximum in any window: newly added sample
		// will always be in all windows the previous samples are. Thus, smaller or
		// equal samples could be removed. This will maintain the invariant - deque
		// contains strictly decreasing sequence of values.
		while (!samples.empty() && samples.back().second <= sample) 
			samples.pop_back();
		// Add the new sample but only if there's no existing sample at the same time.
		// Due to checks above, the already existing element will be larger, so the
		// new sample will never be the maximum in any window.
		if (samples.empty() || samples.back().first < when) 
			samples.emplace_back(std::make_pair(when, sample));
		//Return min value
		return samples.front().second;
	}

	std::optional<T> GetMax() const
	{
		std::optional<T> res;
		if (!samples.empty())
			res.emplace(samples.front().second);
		return res;
	}

	// Advances the current time, and returns the maximum sample in the time
	// window ending at the current time. The new current time must be at least as
	// large as the old current time.
	std::optional<T> Max(uint64_t when) 
	{
		RollWindow(when);
		return GetMax();
	}

	void Reset() 
	{
		samples.clear();
	}

	// Throws out obsolete samples.
	void RollWindow(uint64_t when) 
	{
		if (when<window)
			return;
		//Calculate the start of the current window
		const uint64_t start = when - window;
		//Remove elements 
		while (!samples.empty() && samples.front().first <= start)
			samples.pop_front();
	}

private:
	const uint64_t window;

	// This deque stores (timestamp, sample) pairs in chronological order; new
	// pairs are only ever added at the end. However, because they can't affect
	// the Max() calculation, pairs older than windoware discarded,
	// and if an older pair has a sample that's smaller than that of a younger
	// pair, the older pair is discarded. As a result, the sequence of timestamps
	// is strictly increasing, and the sequence of samples is strictly decreasing.
	std::deque<std::pair<uint64_t, T>> samples;

};


template <class T> 
class MovingMinCounter : 
	private MovingMaxCounter<T>
{
public:
	explicit MovingMinCounter(uint64_t window)
		: MovingMaxCounter<T>(window)
	{
	}

	T Add(uint64_t when, const T& sample)
	{
		if (std::is_unsigned<T>::value)
			return std::numeric_limits<T>::max() - MovingMaxCounter<T>::Add(when, std::numeric_limits<T>::max() - sample);
		else
			return - MovingMaxCounter<T>::Add(when, - sample);
	}	

	std::optional<T> Min(uint64_t when)
	{
		RollWindow(when);
		return GetMin();
	}

	std::optional<T> GetMin() const
	{
		auto res = MovingMaxCounter<T>::GetMax();
		if (!res)
			return res;
		if (std::is_unsigned<T>::value)
			return std::make_optional(std::numeric_limits<T>::max() - *res);
		else
			return std::make_optional(-*res);
	}

	void Reset()
	{
		MovingMaxCounter<T>::Reset();
	}

	void RollWindow(uint64_t when)
	{
		MovingMaxCounter<T>::RollWindow(when);
	}
};

#endif //MOVING_COUNTER_