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
	Acumulator(DWORD window)
	{
		this->window  = window;
		instant = 0;
		Reset();
	}

	QWORD GetAcumulated()	const { return acumulated;	}
	QWORD GetInstant()	const { return instant;		}
	QWORD GetMin()		const { return min;		}
	QWORD GetMax()		const { return max;		}
	DWORD GetWindow()	const { return window;		}
	bool  IsInWindow()	const { return inWindow;	}

	void Reset()
	{
		acumulated = 0;
		max = 0;
		min = (QWORD)-1;
		inWindow = false;
	}
	
	QWORD Update(QWORD now,DWORD val)
	{
		//Update acumulated value
		acumulated += val;
		//And the instant one
		instant += val;
		//Insert into the instant queue
		values.push_back(Value(now,val));
		//Erase old values
		while(values.front().first+window<now)
		{
			//Remove from instant value
			instant -= values.front().second;
			//Delete value
			values.pop_front();
			//We are in a window
			inWindow = true;
		}
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
};
#endif	/* ACUMULATOR_H */

