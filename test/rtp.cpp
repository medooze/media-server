#include "test.h"
#include "rtp.h"

class RTPTestPlan: public TestPlan
{
public:
	RTPTestPlan() : TestPlan("RTP test plan")
	{
		
	}
	
	int init() 
	{
		Log("RTP::Init\n");
		return true;
	}
	
	int end()
	{
		Log("RTP::End\n");
		return true;
	}
	
	
	virtual void Execute()
	{
		init();
		testExtension();
		end();
	}
	
	int testExtension()
	{
		RTPMap	extMap;
		//Set abs extension for testing
		extMap[1] = RTPPacket::HeaderExtension::AbsoluteSendTime;
		
		//RTP payload data
		BYTE recovered[1056];
		DWORD size = 1056;
		//Clean it
		memset(recovered,0,size);
		
		//Get extension header
		rtp_hdr_ext_t* ext = (rtp_hdr_ext_t*)recovered;
		//Set data
		ext->ext_type = htons(0xBEDE);
		//Set total length in 32bits words
		ext->len = htons(1);

		//Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
		// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
		DWORD abs = ((1234 << 18) / 1000) & 0x00ffffff;
		//Set header
		recovered[sizeof(rtp_hdr_ext_t)] = extMap.GetTypeForCodec(RTPPacket::HeaderExtension::AbsoluteSendTime) << 4 | 0x02;
		//Set data
		set3(recovered,sizeof(rtp_hdr_ext_t)+1,abs);
		//Dump
		Dump(recovered,16);
		//Create new video packet
		RTPTimedPacket* packet = new RTPTimedPacket(MediaFrame::Video,96);
		//Set values
		packet->SetP(true);
		packet->SetX(true);
		packet->SetMark(true);
		packet->SetTimestamp(true);
		//Set sequence number
		packet->SetSeqNum(0);
		//Set seq cycles
		packet->SetSeqCycles(0);
		//Set ssrc
		packet->SetSSRC(0xffff);
		//Set payload and recovered length
		if (!packet->SetPayloadWithExtensionData(recovered,size))
			//Error
			return Error("-FEC payload of recovered packet to big [%u]\n",(unsigned int)size);
		//Process extensions
		packet->ProcessExtensions(extMap);
		//Dump
		packet->Dump();
		//Check
		if (ntohs(packet->GeExtensionHeader()->len)!=1)
			//Error
			return Error("-Incorrect extension length %d, should be 1\n",packet->GetExtensionLength());
		//Clean it
		delete packet;
		//OK
		return true;
	}
	
};

RTPTestPlan rtp;
