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

/*
	Acumulator类:
		主要用于统计单位时间内，接收到的音视频数据大小(单位:字节)，可用于计算瞬时发送或者接收速度
	核心成员values为list结构, 其中first : time, second : bytes大小。
	采用的类似于queue的结构，先收到的数据存储在front,后收到的数据存储在back
*/

class Acumulator
{
public:
	Acumulator(DWORD window)
	{
		this->window  = window;
		instant = 0;
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
	long double GetInstantAvg()	const { return GetInstant()*1000/GetWindow();	}
	long double GetAverage()	const { return GetDiff() ? GetAcumulated()*1000/GetDiff() : 0;	}
	long double GetMinAvg()		const { return GetMin()*1000/GetWindow();	}
	long double GetMaxAvg()		const { return GetMax()*1000/GetWindow();	}

	void ResetMinMax()
	{
		max = 0;
		min = (QWORD)-1;
	}

	void Reset(QWORD now)
	{
		acumulated = 0;
		max = 0;
		min = (QWORD)-1;
		first = now;
		last = 0;
		inWindow = false;
	}
	
	QWORD Update(QWORD now,DWORD val = 0)
	{
		//Update acumulated value
		acumulated += val;
		//And the instant one
		instant += val;
		//Insert into the instant queue
		values.push_back(Value(now,val));
		//Erase old values
		while(!values.empty() && values.front().first+window<now)
		{
			//Remove from instant value
			instant -= values.front().second;
			//Delete value
			values.pop_front();
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
		return values.size();
	}
	

private:
	typedef std::pair<QWORD,DWORD>  Value;
	typedef std::list<Value>	Values;
private:
	Values values;
	DWORD window;
	bool  inWindow;
	QWORD acumulated;
	QWORD instant;
	QWORD max;
	QWORD min;
	QWORD first;
	QWORD last;
};
#endif	/* ACUMULATOR_H */

