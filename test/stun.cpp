#include <memory>
#include "test.h"
#include "stunmessage.h"

class StunPlan: public TestPlan
{
public:
	StunPlan() : TestPlan("STUN test plan")
	{
		
	}
	
	
	virtual void Execute()
	{
		testAuth();
	}
	
	void testAuth()
	{
		//Create trans id
		BYTE transId[12];
		//Set first to 0
		set4(transId,0,0);
		//Set timestamp as trans id
		set8(transId,4,getTime());
		//Create binding request to send back
		auto request = std::make_unique<STUNMessage>(STUNMessage::Request,STUNMessage::Binding,transId);
		//Add username
		request->AddUsernameAttribute("localusername","remoteusername");
		//Add other attributes
		request->AddAttribute(STUNMessage::Attribute::IceControlled,(QWORD)1);
		request->AddAttribute(STUNMessage::Attribute::Priority,(DWORD)33554431);

		request->Dump();
		//Create new mesage
		uint8_t data[1025];
		size_t  size = sizeof(data);
		
		//Serialize and autenticate
		size_t len = request->AuthenticatedFingerPrint(data,size,"pwd");
		
		//Parse
		auto parsed = STUNMessage::Parse(data,len);
		
		parsed->Dump();
		//Ensure it is athenticated correctly
		assert(parsed->CheckAuthenticatedFingerPrint(data,size,"pwd"));
		
		delete parsed;
		
	}
	
};

StunPlan stun;

