#include "test.h"
#include "vp9/VP9PayloadDescription.h"

class VP9Plan: public TestPlan
{
public:
	VP9Plan() : TestPlan("VP9 test plan")
	{
		
	}
	
	
	virtual void Execute()
	{
		testDescriptor();
	}
	
	void testDescriptor()
	{
		
		BYTE aux[1024];
		BYTE data[] = { 
			0x8a,
			0x80,   0x2a,
			
			0x18, // 
			0x02, // W
			0x80,
			0x01, // H
			0xe0,
			//GOB
			0x01,
			0x04,
			0x01,
		};

		DWORD size = sizeof(data);
		
		VP9PayloadDescription desc;
		
		DWORD len = desc.Parse(data,size);
		desc.Dump();
		len = desc.Serialize(aux,1024);
		Dump(data,size);
		Dump(aux,len);
	}
	
};

VP9Plan vp9;
