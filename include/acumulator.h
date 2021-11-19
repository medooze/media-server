/* 
 * File:   acumulator.h
 * Author: Sergio
 *
 * Created on 26 de diciembre de 2012, 13:10
 */

#ifndef ACUMULATOR_H
#define	ACUMULATOR_H

#include <stdint.h>
#include "CircularQueue.h"

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

	uint64_t GetAcumulated()	const { return acumulated;			}
	uint64_t GetDiff()		const { return last-first;			}
	uint64_t GetInstant()		const { return instant;				}
	uint64_t GetMin()		const { return min;				}
	uint64_t GetMax()		const { return max;				}
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
		min = std::numeric_limits<uint64_t>::max();
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
	}

	uint64_t Update(uint64_t now)
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
		//Return accumulated value
		return instant;
	}
	
	uint64_t Update(uint64_t now, uint32_t val)
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
		//Return accumulated value
		return instant;
	}
	
	uint32_t GetMinValueInWindow() const
	{
		uint32_t minValue = std::numeric_limits<uint32_t>::max();
		//For eacn entry
		for (size_t i = 0; i < values.length(); ++i)
		{
			const auto& value = values.at(i);
			//If it is less
			if (value.second<minValue)
				//Store it
				minValue = value.second;
		}
		//Return minimum value in window
		return minValue;
	}
	
	uint32_t GetMaxValueInWindow() const
	{
		uint32_t maxValue = 0;
		//For eacn entry
		for (size_t i = 0; i < values.length(); ++i)
		{
			const auto& value = values.at(i);
			//If it is more
			if (value.second>maxValue)
				//Store it
				maxValue = value.second;
		}
		//Return maxi value in window
		return maxValue;
	}
	
	std::pair<uint32_t,uint32_t> GetMinMaxValueInWindow() const
	{
		uint32_t minValue = std::numeric_limits<uint32_t>::max();
		uint32_t maxValue = 0;
		//For eacn entry
		for (size_t i = 0; i < values.length(); ++i)
		{
			const auto& value = values.at(i);
			//If it is more
			if (value.second > maxValue)
				//Store it
				maxValue = value.second;
			//If it is less
			if (value.second < minValue)
				//Store it
				minValue = value.second;
		}
		//Return min max values in window
		return {minValue,maxValue};
	}

	uint32_t GetCount() const
	{
		return count;
	}
	
	uint32_t IsEmpty() const
	{
		return values.empty();
	}
private:
	CircularQueue<std::pair<uint64_t,uint32_t>> values;
	uint32_t count;
	uint32_t window;
	uint32_t base;
	bool  inWindow;
	uint64_t acumulated;
	uint64_t instant;
	uint64_t max;
	uint64_t min;
	uint64_t first;
	uint64_t last;
};
#endif	/* ACUMULATOR_H */

