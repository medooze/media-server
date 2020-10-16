#include "test.h"
#include "rtp.h"
#include "BitHistory.h"

class RTPTestPlan: public TestPlan
{
public:
	RTPTestPlan() : TestPlan("Dependency layer selector test plan")
	{
	}
	
	int init() 
	{
		Log("DDLS::Init\n");
		return true;
	}
	
	int end()
	{
		Log("DDLS::End\n");
		return true;
	}
	
	
	virtual void Execute()
	{
		init();
		
		Log("testBitHistory\n");
		testBitHistory();
		
		end();
	}
	
	virtual void testBitHistory()
	{
		{
			BitHistory<256> history;

			history.Add(1);
			assert(history.Contains(1));
			assert(history.ContainsOffset(0));

			history.Add(2);
			assert(history.Contains(1));
			assert(history.ContainsOffset(1));

			history.Add(4);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(3));
			assert(!history.ContainsOffset(1));

			history.Add(4);
			assert(history.Contains(1));
			assert(history.ContainsOffset(3));

			history.Add(65);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(64));

			history.Add(67);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(65));

			history.Add(240);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(239));
		}
		
		{
			BitHistory<256> history;

			for (int i = 1; i<=64;++i)
				history.Add(i);
			
			for (int i = 64; i<=256+64;++i)
				history.Add(i);
			
			
			history.Add(256+64+64);
			assert(history.Contains(256+64));
			assert(!history.Contains(256+64+1));
			
		}
	}
};

RTPTestPlan ddls;
