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
		base(base),
		minCounter(base),
		maxCounter(base)
	{
		Reset(0);
	}

	T GetAcumulated()		const { return acumulated;			}
	T GetInstant()			const { return instant;				}
	T GetMin()			const { return min;				}
	T GetMax()			const { return max;				}
	uint64_t GetDiff()		const { return last - first;			}
	uint32_t GetWindow()		const { return window;				}
	bool  IsInWindow()		const { return inWindow;			}
	bool  IsInMinMaxWindow()	const { return inWindow && min!=(uint64_t)-1;	}
	long double GetInstantMedia()	const { return GetCount() ? GetInstant()/GetCount() : 0;	}
	long double GetInstantAvg()	const { return GetInstant()*base/GetWindow();			}
	long double GetAverage()	const { return GetDiff() ? GetAcumulated()*base/GetDiff() : 0;	}
	long double GetMinAvg()		const { return GetMin()*base/GetWindow();			}
	long double GetMaxAvg()		const { return GetMax()*base/GetWindow();			}

	void ResetMinMax()
	{
		max = 0;
		min = std::numeric_limits<T>::max();
	}

	void Reset(uint64_t now)
	{
		instant = 0;
		acumulated = 0;
		max = 0;
		min = std::numeric_limits<uint64_t>::max();
		first = now;
		last = 0;
		inWindow = false;
		values.clear();
		count = 0;
		maxCounter.Reset();
		minCounter.Reset();
	}

	T Update(uint64_t now)
	{
		//Erase old values
		while (!values.empty() && values.front().first + window <= now)
		{
			//Ensure we don't overflow (should not happen)
			if (instant >= values.front().second)
				//Remove from instant value
				instant -= values.front().second;
			else 
				//Error
				instant = 0; //assert(false);
			//Delete value
			values.pop_front();
			count--;
			//We are in a window
			inWindow = true;
		}
		//Check max
		if (instant > max)
			//new max
			max = instant;
		//Check min in window
		if (inWindow && instant < min)
			//New min
			min = instant;
		//Set min and max moving counters
		minCounter.RollWindow(now);
		maxCounter.RollWindow(now);
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
		//Insert into the instant queue
		values.emplace_back(now,val);
		count++;
		//Erase old values
		while(!values.empty() && values.front().first+window<=now)
		{
			//Ensure we don't overflow (should not happen)
			if (instant >= values.front().second)
				//Remove from instant value
				instant -= values.front().second;
			else
				//Error
				instant = 0; //assert(false);
			//Delete value
			values.pop_front();
			count--;
			//We are in a window
			inWindow = true;
		}
		//If we do not have first
		if (!first)
			//This is first
			first = now;
		//Update last
		last = now;
		//Check max
		if (instant>max)
			//new max
			max = instant;
		//Check min in window
		if (inWindow && instant<min)
			//New min
			min = instant;
		//Set min and max moving counters
		minCounter.RollWindow(now);
		maxCounter.RollWindow(now);
		//Return accumulated value
		return instant;
	}
	
	V GetMinValueInWindow() const
	{
		//Return minimum value in window
		return minCounter.GetMin().value_or(std::numeric_limits<V>::max());
	}
	
	V GetMaxValueInWindow() const
	{
		//Return max value in window
		return maxCounter.GetMax().value_or(std::numeric_limits<V>::min());
	}
	
	std::pair<V,V> GetMinMaxValueInWindow() const
	{
		//Return min max values in window
		return { GetMinValueInWindow(), GetMaxValueInWindow() };
	}

	uint32_t GetCount() const
	{
		return count;
	}
	
	bool IsEmpty() const
	{
		return values.empty();
	}
private:
	CircularQueue<std::pair<uint64_t,V>> values;

	uint32_t count;
	uint32_t window;
	uint32_t base;
	bool  inWindow;
	T acumulated;
	T instant;
	T max;
	T min;
	uint64_t first;
	uint64_t last;

	MovingMinCounter<V> minCounter;
	MovingMaxCounter<V> maxCounter;
};
#endif	/* ACUMULATOR_H */

