#include "test.h"
#include "cpim.h"


class CPIMTestPlan: public TestPlan
{
public:
	CPIMTestPlan() : TestPlan("CPIM test plan")
	{
		
	}
	
	int message() 
	{
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
		//Test serializing
		if (!len) return Error("CPIM message not serialized\r\n");
		//Dump message
		Dump(data,len);

		//Now clone it
		CPIMMessage* cloned = new CPIMMessage(cpim->GetFrom().GetURI(),cpim->GetTo().GetURI(),cpim->GetContent()->Clone()); 

		//Dump
		cloned->Dump();

		//Serialize it
		len = cloned->Serialize(data,size);
		//Test serializing
		if (!len) return Error("CPIM cloned message not serialized\r\n");
		//Dump message
		Dump(data,len);
		//Free data
		free(data);
			
		//Delete
		delete cpim;
		delete cloned;
		
		//OK
		return true;
	}

	int iscomposing() 
	{
		const char* msg = 
			"From: <im:502@42007bac-b25b-4c52-bc79-6dbaa10de997-ff7fe27e-3dcc-493f-b029-0a4d617f5b47>\r\n"
			"To: <im:0@42007bac-b25b-4c52-bc79-6dbaa10de997-ff7fe27e-3dcc-493f-b029-0a4d617f5b47>\r\n"
			"DateTime: 2015-10-09T13:20:12.110Z\r\n"
			"\r\n"
			"Content-Type: application/im-iscomposing+json\r\n"
			"\r\n"
			"{\r\n"
			" \"state\": \"active\",\r\n"
			" \"contentType\": \"text\",\r\n"
			" \"refresh\": 45\r\n"
			"}";
		
		//Parse it
		CPIMMessage* cpim = CPIMMessage::Parse((BYTE*)msg,strlen(msg));
		
		//Test parsing
		if (!cpim) return Error("CPIM iscomposing not parsed\r\n");
		
		//Dump
		cpim->Dump();
		
		//Test serialization
		DWORD size = strlen(msg);
		BYTE *data = (BYTE*)malloc(size);
		//Serialize it
		int len = cpim->Serialize(data,size);
		//Test serializing
		if (!len) return Error("CPIM iscomposing not serialized\r\n");
		//Dump message
		Dump(data,len);

		//Now clone it
		CPIMMessage* cloned = new CPIMMessage(cpim->GetFrom().GetURI(),cpim->GetTo().GetURI(),cpim->GetContent()->Clone()); 

		//Dump
		cloned->Dump();

		//Serialize it
		len = cloned->Serialize(data,size);
		//Test serializing
		if (!len) return Error("CPIM cloned iscomposing not serialized\r\n");
		//Dump message
		Dump(data,len);
		//Free data
		free(data);
			
		//Delete
		delete cpim;
		delete cloned;
		
		//OK
		return true;
	}

	int contentType() 
	{
		ContentType* type = ContentType::Parse("text/plain ; charset = utf-8");

		//Check it is parsed corretly
		if (!type)
			return Error("ContentType not parsed correctly\n");

		Debug("ContentType: \"%s\"\n" , type->ToString().c_str());

		//Clone
		ContentType* cloned = type->Clone();

		Debug("ContentType: \"%s\"\n" , cloned->ToString().c_str());

		delete cloned;	
		delete type;
	}
	
	virtual void Execute()
	{
		message();
		iscomposing();
		contentType();
	}
	
};

//CPIMTestPlan cpim;
