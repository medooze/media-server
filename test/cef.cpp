#include "test.h"


#ifdef CEF

#include "cef/Browser.h"

class CEFTestPlan: public TestPlan
{
public:
	CEFTestPlan() : TestPlan("CPIM test plan")
	{
		
	}
	
	int init() 
	{
		Log("CEFTestPlan::Init\n");
		return Browser::getInstance().Init();
	}
	
	int end()
	{
		Log("CEFTestPlan::End\n");
		return Browser::getInstance().End();
	}
	
	
	virtual void Execute()
	{
		init();
		end();
	}
	
};

CEFTestPlan cefp;
#endif 
