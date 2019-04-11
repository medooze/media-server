#include "test.h"
#include "aac/aacconfig.h"
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
		testAACConfig();
	}
	
	void testEncoder()
	{
		Properties props;
		AACEncoder encoder(props);
	}
	
	void testAACConfig()
	{
		BYTE aux[7] = {};
		size_t len;
		BYTE aac1[2] = { 0x16, 0x08 };
		BYTE aac2[2] = { 0x11, 0x88 };

		AACSpecificConfig config1;
		AACSpecificConfig config2;
		
		config1.Decode(aac1,sizeof(aac1));
		config1.Dump();
		len = config1.Serialize(aux,sizeof(aux));
		Dump(aux,len);
		
		config2.Decode(aac2,sizeof(aac2));
		config2.Dump();
		len = config2.Serialize(aux,sizeof(aux));
		Dump(aux,len);
		
		
		
		AACSpecificConfig config3(48000,1);
		config3.Dump();
		len = config3.Serialize(aux,sizeof(aux));
		Dump(aux,len);
		
		Log("%d %d %d\n",config1.GetRate(),config2.GetRate(),config3.GetRate());
	}
	
};

AACTestPlan aac;

