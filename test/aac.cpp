#include "test.h"
#include "aac/aacencoder.h"

class AACTestPlan: public TestPlan
{
public:
	AACTestPlan() : TestPlan("AAC test plan")
	{
		
	}
	
	
	virtual void Execute()
	{
		testEncoder();
	}
	
	void testEncoder()
	{
		Properties props;
		AACEncoder encoder(props);
	}
	
};

AACTestPlan aac;

