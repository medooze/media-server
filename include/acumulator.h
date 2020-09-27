/* 
 * File:   acumulator.h
 * Author: Sergio
 *
 * Created on 26 de diciembre de 2012, 13:10
 */

#ifndef ACUMULATOR_H
#define	ACUMULATOR_H

#include "config.h"
#include <list>

class Acumulator
{
public:
	Acumulator(DWORD window, DWORD base = 1000) :
		window(window),
		base(base)
	{
		Reset(0);
	}

	QWORD GetAcumulated()		const { return acumulated;			}
	QWORD GetDiff()			const { return last-first;			}
	QWORD GetInstant()		const { return instant;				}
	QWORD GetMin()			const { return min;				}
	QWORD GetMax()			const { return max;				}
	DWORD GetWindow()		const { return window;				}
	bool  IsInWindow()		const { return inWindow;			}
	bool  IsInMinMaxWindow()	const { return inWindow && min!=(QWORD)-1;	}
	long double GetInstantMedia()	const { return GetCount() ? GetInstant()/GetCount() : 0;	}
	long double GetInstantAvg()	const { return GetInstant()*base/GetWindow();			}
	long double GetAverage()	const { return GetDiff() ? GetAcumulated()*base/GetDiff() : 0;	}
	long double GetMinAvg()		const { return GetMin()*base/GetWindow();			}
	long double GetMaxAvg()		const { return GetMax()*base/GetWindow();			}

	void ResetMinMax()
	{
		max = 0;
		min = (QWORD)-1;
	}

	void Reset(QWORD now)
	{
		instant = 0;
		acumulated = 0;
		max = 0;
		min = (QWORD)-1;
		first = now;
		last = 0;
		inWindow = false;
		values.clear();
		count = 0;
	}
	
	QWORD Update(QWORD now,DWORD val = 0)
	{
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
			//Remove from instant value
			instant -= values.front().second;
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
	
	DWORD GetMinValueInWindow() const
	{
		DWORD minValue = 0xFFFF;
		//For eacn entry
		for (const auto& value: values)
			//If it is less
			if (value.second<minValue)
				//Store it
				minValue = value.second;
		//Return minimum value in window
		return minValue;
	}
	
	DWORD GetMaxValueInWindow() const
	{
		DWORD maxValue = 0;
		//For eacn entry
		for (const auto& value: values)
			//If it is more
			if (value.second>maxValue)
				//Store it
				maxValue = value.second;
		//Return maxi value in window
		return maxValue;
	}
	
	DWORD GetCount() const
	{
		return count;
	}
	
	DWORD IsEmpty() const
	{
		return values.empty();
	}
private:
	std::list<std::pair<QWORD,DWORD>> values;
	DWORD count;
	DWORD window;
	DWORD base;
	bool  inWindow;
	QWORD acumulated;
	QWORD instant;
	QWORD max;
	QWORD min;
	QWORD first;
	QWORD last;
};
#endif	/* ACUMULATOR_H */

