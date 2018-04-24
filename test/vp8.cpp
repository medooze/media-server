#include "test.h"
#include "vp8/vp8.h"

class VP8Plan: public TestPlan
{
public:
	VP8Plan() : TestPlan("VP8 test plan")
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
			0x90,
			0xe0,
			0xc7,
			0x12,
			0x18,
			0xa0
		};

		DWORD size = sizeof(data);
		
		VP8PayloadDescriptor desc;
		
		DWORD len = desc.Parse(data,size);
		desc.Dump();
		len = desc.Serialize(aux,1024);
		Dump(data,size);
		Dump(aux,len);
		
		VP8PayloadDescriptor desc2;
		desc.Parse(aux,len);
		desc.Dump();
		
		desc.pictureId = 1;
		desc.Dump();
		len = desc.Serialize(aux,1024);
		Dump(aux,len);
	}
	
};

VP8Plan vp8;

