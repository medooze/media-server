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
		testTransportField();
		testExtension();
		
		end();
	}
	
	int testExtension()
	{
		RTPMap	extMap;
		//Set abs extension for testing
		extMap[1] = RTPHeaderExtension::AbsoluteSendTime;
		
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
		recovered[sizeof(rtp_hdr_ext_t)] = extMap.GetTypeForCodec(RTPHeaderExtension::AbsoluteSendTime) << 4 | 0x02;
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
	
	void testTransportField()
	{
		BYTE data[] = { 
		0x8f,0xcd,0x00,0x05,
		0x29,0xdd,0x06,0xa4,
		0x00,0x00,0x27,0x50,
		0x04,0x1e,0x00,0x02,
		0x09,0x03,0x95,0xed,
		0xc4,0x00,0x0c,0x00 };

		DWORD size = sizeof(data);
		
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(data,size);
		rtcp->Dump();
		
		delete(rtcp);
		
		BYTE data2[] = {
		0x8f,0xcd,0x00,0x05,
		0x97,0xb3,0x78,0x4f,
		0x00,0x00,0x40,0x4f,
		
		0x00,0x45,0x00,0x01,
		0x09,0xf9,0x73,0x00,
		0x20,0x01,0x0c,0x00};

		DWORD size2 = sizeof(data2);
		
		//Parse it
		RTCPCompoundPacket* rtcp2 = RTCPCompoundPacket::Parse(data2,size2);
		rtcp2->Dump();
		
		delete(rtcp2);
		
	}
	
};

RTPTestPlan rtp;
