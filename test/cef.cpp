#include "test.h"
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
		return browser.Init();
	}
	
	int end()
	{
		Log("CEFTestPlan::End\n");
		return browser.End();
	}
	
	
	virtual void Execute()
	{
		init();
		end();
	}
	
	Browser browser;
	
};

CEFTestPlan cefp;