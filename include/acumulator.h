/* 
 * File:   acumulator.h
 * Author: Sergio
 *
 * Created on 26 de diciembre de 2012, 13:10
 */

#ifndef ACUMULATOR_H
#define	ACUMULATOR_H

#include <stdint.h>
#include <limits>
#include "CircularQueue.h"
#include "MovingCounter.h"

template <typename  V = uint32_t, typename T = uint64_t>
class Acumulator
{
public:
	Acumulator(uint32_t window, uint32_t base = 1000, uint32_t initialSize = 0) :
		values(initialSize),
		window(window),
		base(base)
	{
		Reset(0);
	}

	T GetAcumulated()		const { return acumulated;			}
	T GetInstant()			const { return instant;				}
	uint64_t GetDiff()		const { return last - first;			}
	uint32_t GetWindow()		const { return window;				}
	bool  IsInWindow()		const { return inWindow;			}
	long double GetInstantMedia()	const { return GetCount() ? GetInstant()/GetCount() : 0;	}
	long double GetInstantAvg()	const { return GetInstant()*base/GetWindow();			}
	long double GetAverage()	const { return GetDiff() ? GetAcumulated()*base/GetDiff() : 0;	}

	void Reset(uint64_t now)
	{
		instant = 0;
		acumulated = 0;
		first = now;
		last = 0;
		inWindow = false;
		values.clear();
		count = 0;
	}

	T Update(uint64_t now)
	{
		//Erase old values
		while (!values.empty())
		{
			//Get first item
			const auto& front = values.front();

			//Check if value is in the time  window
			if (now < window || front.timestamp > (now - window))
				//Stop removal
				break;
			//Ensure we don't overflow for unsigned types (should not happen)
			if (!std::is_unsigned<T>::value || instant >= front.value)
				//Remove from instant value
				instant -= front.value;
			else
				//Error
				instant = 0; //assert(false);

			//Remove value value
			count -= front.count;

			//Delete value
			values.pop_front();

			//We are in a window
			inWindow = true;
		}
		//Return accumulated value
		return instant;
	}
	
	T Update(uint64_t now, V val)
	{
		//Don't allow time to go back
		if (now<last)
			now = last;
		//Update acumulated value
		acumulated += val;
		//And the instant one
		instant += val;
		//Check if last item has the same timestamp
		if (!values.empty())
		{
			//Get last item
			auto& back = values.back();

			//If it is happening at the same timestamp
			if (back.timestamp == now)
			{
				//Increase value and counter on existing value
				back.count ++;
				back.value +=val;
			} else {
				//Insert new value
				values.emplace_back(now, 1, val);
			}
		} else {
			//Insert new value
			values.emplace_back(now, 1, val);
		}
		//Increase global window
		count++;
		//If we do not have first
		if (!first)
			//This is first
			first = now;
		//Update last
		last = now;
		//Erase old values and Return accumulated value
		return Update(now);
	}

	uint32_t GetCount() const
	{
		return count;
	}
	
	bool IsEmpty() const
	{
		return values.empty();
	}
protected:
	struct Value {
		Value( uint64_t timestamp, uint32_t count, V value) :
			timestamp(timestamp),
			count(count),
			value(value)
		{}
		Value() = default;
		uint64_t timestamp = 0;
		uint32_t count = 0;
		V	 value = 0; 
	};

	CircularQueue<Value> values;

	uint32_t count;
	uint32_t window;
	uint32_t base;
	bool  inWindow;
	T acumulated;
	T instant;
	uint64_t first;
	uint64_t last;
};

template <typename  V = uint32_t, typename T = uint64_t>
class MaxAcumulator : public Acumulator<V, T>
{
public:
	MaxAcumulator(uint32_t window, uint32_t base = 1000, uint32_t initialSize = 0) :
		Acumulator<V, T>(window, base, initialSize),
		maxCounter(window)
	{
		ResetMax();
	}
	T GetMax()			const { return max; }
	bool  IsInMaxWindow()		const { return this->inWindow; }
	long double GetMaxAvg()		const { return GetMax() * this->base / this->GetWindow(); }

	void ResetMax()
	{
		max = 0;
	}

	void Reset(uint64_t now)
	{
		Acumulator<V, T>::Reset(now);

		max = 0;
		maxCounter.Reset();
	}

	T Update(uint64_t now)
	{
		Acumulator<V, T>::Update(now);

		//Check max
		if (this->instant > max)
			//new max
			max = this->instant;
		//Set max moving counters
		maxCounter.RollWindow(now);
		//Return accumulated value
		return this->instant;
	}

	T Update(uint64_t now, V val)
	{
		Acumulator<V, T>::Update(now, val);

		//Check max
		if (this->instant > max)
			//new max
			max = this->instant;
		//Set min and max moving counters
		maxCounter.Add(now, val);
		//Return accumulated value
		return this->instant;
	}


	V GetMaxValueInWindow() const
	{
		//Return max value in window
		return maxCounter.GetMax().value_or(std::numeric_limits<V>::min());
	}


private:
	T max;
	MovingMaxCounter<V> maxCounter;
};

template <typename  V = uint32_t, typename T = uint64_t>
class MinMaxAcumulator : public MaxAcumulator<V, T>
{
public:
	MinMaxAcumulator(uint32_t window, uint32_t base = 1000, uint32_t initialSize = 0) : 
		MaxAcumulator<V,T>(window, base, initialSize),
		minCounter(window)
	{
		ResetMinMax();
	}
	T GetMin()			const { return min; }
	bool  IsInMinMaxWindow()	const { return this->inWindow && min != std::numeric_limits<T>::max(); }
	long double GetMinAvg()		const { return GetMin() * this->base / this->GetWindow(); }

	void ResetMinMax()
	{
		MaxAcumulator<V, T>::ResetMax();
		min = std::numeric_limits<T>::max();
	}

	void Reset(uint64_t now)
	{
		MaxAcumulator<V, T>::Reset(now);

		min = std::numeric_limits<T>::max();
		minCounter.Reset();
	}

	T Update(uint64_t now)
	{
		MaxAcumulator<V, T>::Update(now);

		//Check min in window
		if (this->inWindow && this->instant < min)
			//New min
			min = this->instant;
		//Set min counters
		minCounter.RollWindow(now);
		//Return accumulated value
		return this->instant;
	}

	T Update(uint64_t now, V val)
	{
		MaxAcumulator<V, T>::Update(now, val);

		//Check min in window
		if (this->inWindow && this->instant < min)
			//New min
			min = this->instant;
		//Set min counters
		minCounter.Add(now, val);
		//Return accumulated value
		return this->instant;
	}

	V GetMinValueInWindow() const
	{
		//Return minimum value in window
		return minCounter.GetMin().value_or(std::numeric_limits<V>::max());
	}

	std::pair<V, V> GetMinMaxValueInWindow() const
	{
		//Return min max values in window
		return { GetMinValueInWindow(), MaxAcumulator<V, T>::GetMaxValueInWindow() };
	}

private:
	T min;
	MovingMinCounter<V> minCounter;
};

#endif	/* ACUMULATOR_H */

