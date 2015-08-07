#include "test.h"
#include "cpim.h"


class CPIMTestPlan: public TestPlan
{
public:
	CPIMTestPlan() : TestPlan("CPIM test plan")
	{
		
	}
	
	int message() {
		const char* msg = 
			"From: Inaki Baz Castillo <im:inaki.baz@eface2face.com>\r\n" 
			"To: Alicia <im:alicia@atlanta.com>\r\n" 
			"Subject: Urgent, read pliz pliz\r\n" 
			"DateTime: 2000-12-13T21:40:00.000Z\r\n" 
			"\r\n" 
			"Content-Type: text/plain ; charset = utf-8\r\n" 
			"Content-Length: 5\r\n" 
			"\r\n" 
			"HELLO";
		
		//Parse it
		CPIMMessage* cpim = CPIMMessage::Parse((BYTE*)msg,strlen(msg));
		
		//Test parsing
		if (!cpim) return Error("CPIM message not parsed\r\n");
		
		//Dump
		cpim->Dump();
		
		//Test serialization
		DWORD size = strlen(msg);
		BYTE *data = (BYTE*)malloc(size);
		//Serialize it
		int len = cpim->Serialize(data,size);
		//Dump message
		Dump(data,len);
		//Free data
		free(data);
			
		//Delete
		delete cpim;
		
		//OK
		return true;
	}
	
	virtual void Execute()
	{
		message();
	}
	
};

CPIMTestPlan cpim;