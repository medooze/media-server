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
		Log("RTPHeader\n");
		testRTPHeader();
		Log("testSenderReport\n");
		testSenderReport();
		Log("NACK\n");
		testNack();
		testTransportField();
		end();
	}
	
	int testRTPHeader()
	{
		RTPHeader header;
		RTPHeaderExtension extension;
		RTPMap	extMap;
		extMap[5] = RTPHeaderExtension::TransportWideCC;
		
		BYTE data[]= {	
				0x90,0xe2,0x7c,0x2d,0x96,0x92,0x68,0x3a,0x00,0x00,0x62,0x32, //RTP Header
				0xbe,0xde,0x00,0x01,0x51,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
		DWORD size = sizeof(data);
		
		//Parse header
		DWORD len = header.Parse(data,size);
		
		//Dump
		header.Dump();
		
		//Check data
		assert(len);
		assert(header.version==2);
		assert(!header.padding);
		assert(header.extension);
		assert(header.payloadType==98);
		assert(header.csrcs.size()==0);
		assert(header.ssrc==0x6232);
		assert(header.mark);
		assert(header.timestamp=252617738);
		
		//Parse extensions
		len = extension.Parse(extMap,data+len,size-len);
		
		//Dump
		extension.Dump();
		
		//Check
		assert(len);
		assert(extension.hasTransportWideCC);
	}
	

	void testSenderReport()
	{
		BYTE msg[] = {
			0x80,  0xc8,  0x00,  0x06,
			
			0x95,  0xce,  0x79,  0x1d,  //<-Sender
			0xdc,  0x48,  0x24,  0x68,  //NTP ts
			0xf7,  0x8e,  0xb0,  0x32, 
			0xe3,  0x4b,  0xae,  0x04,  //RTP ts
			0x00,  0x00,  0x00,  0xec,  //sende pc
			0x00,  0x01,  0x97,  0x22,  //sender oc
			
			0x81,  0xca,  0x00,  0x06,  //SSRC 1
			0x95,  0xce,  0x79,  0x1d,  //
			0x01,  0x10,  0x43,  0x4a,  //
			0x54,  0x62,  0x63,  0x47,  // 
			0x48,  0x36,  0x50,  0x59,  // 
			0x44,  0x4c,  0x43,  0x6c,  // 
			0x69,  0x34,  0x00,  0x00,  //
		};
		
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(msg,sizeof(msg));
		rtcp->Dump();
		
		//Serialize
		BYTE data[1024];
		DWORD len = rtcp->Serialize(data,1024);
		Dump(data,len);
		
		assert(RTCPCompoundPacket::IsRTCP(data,len));
		assert(len==sizeof(msg));
		assert(memcmp(msg,data,len)==0);
		
		//Dump it
		rtcp->Dump();
		
		delete(rtcp);

	}
	
	void testNack()
	{
		
		BYTE msg[] = {
				0x81,   0xcd,   0x00,   0x03,  0x8a,   0xd7,   0xb4,   0x92, 
				0x00,  0x00,   0x61,   0x3b,   0x10,   0x30,   0x3,   0x00 
		};
			/*
			[RTCPCompoundPacket count=1 size=20]
			[RTCPPacket Feedback NACK sender:2329392274 media:24891]
				   [NACK pid:4144 blp:0000001100000000 /]
			[/RTCPPacket Feedback NACK]
			[/RTCPCompoundPacket]
			 */
		BYTE data[1024];
		
		//Create rtcp sender retpor
		RTCPCompoundPacket rtcp;

		//Create NACK
		RTCPRTPFeedback *nack = RTCPRTPFeedback::Create(RTCPRTPFeedback::NACK,2329392274,24891);

		//Add 
		nack->AddField(new RTCPRTPFeedback::NACKField(4144,(WORD)0x0300));

		//Add to packet
		rtcp.AddRTCPacket(nack);
		
		rtcp.Dump();
		
		//Serialize it
		DWORD len = rtcp.Serialize(data,1024);
		
		Dump(data,len);
		
		assert(RTCPCompoundPacket::IsRTCP(data,len));
		assert(len==sizeof(msg));
		assert(memcmp(msg,data,len)==0);
		
		//parse it back
		RTCPCompoundPacket* cloned = RTCPCompoundPacket::Parse(data,len);
		
		assert(cloned);
		
		//Serialize it again
		len = rtcp.Serialize(data,1024);
		
		Dump(data,len);
		
		assert(RTCPCompoundPacket::IsRTCP(data,len));
		assert(len==sizeof(msg));
		assert(memcmp(msg,data,len)==0);
		
		//Dump it
		cloned->Dump();
		
		delete(cloned);
	}
	
	void testTransportField()
	{
		BYTE aux[1024];
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
		
		//Serialize
		DWORD len = rtcp->Serialize(aux,1024);
		Dump(data,sizeof(data));
		Dump(aux,len);
		
		RTCPCompoundPacket* cloned = RTCPCompoundPacket::Parse(aux,len);
		cloned->Dump();
		
		delete(rtcp);
		delete(cloned);
		
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
		

		len = rtcp2->Serialize(aux,1024);
		Dump(data2,sizeof(data));
		Dump(aux,len);
		RTCPCompoundPacket* cloned2 = RTCPCompoundPacket::Parse(aux,len);
		cloned2->Dump();
		
		delete(rtcp2);
		delete(cloned2);
		
	}
	
};

RTPTestPlan rtp;
