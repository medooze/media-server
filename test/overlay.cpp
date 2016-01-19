#include "test.h"
#include "overlay.h"



class OverlayTestPlan: public TestPlan
{
public:
	OverlayTestPlan() : TestPlan("Overlay test plan")
	{
		
	}
	
	int canvas() 
	{
		Canvas* canvas = new Canvas(768,576);

		canvas->LoadPNG("recording-overlay.png");

		delete(canvas);
		
		//OK
		return true;
	}

	
	virtual void Execute()
	{
		canvas();
	}
	
};

OverlayTestPlan overlay;
