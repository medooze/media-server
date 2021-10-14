#include "test.h"
#include "acumulator.h"
#include "MovingCounter.h"

class ToolsPlan : public TestPlan
{
public:
	ToolsPlan() : TestPlan("Tools test plan")
	{

	}

	virtual void Execute()
	{
		testAccumulator();
	}

	void testAccumulator()
	{

		Acumulator acu(10);
		MovingMinCounter<DWORD> min(10);
		MovingMaxCounter<DWORD> max(10);

		
		for (uint64_t i = 10; i < 1E6; i++)
		{
			DWORD val = rand();
			acu.Update(i, val);
			min.Add(i, val);
			max.Add(i, val);

			assert(acu.GetMaxValueInWindow() == *max.GetMax());
			assert(acu.GetMinValueInWindow() == *min.GetMin());
			
			
		}	
	}

};

ToolsPlan tools;

