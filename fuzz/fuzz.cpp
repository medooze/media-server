#include <set>
#include <string>
#include <ostream>  
#include <iostream>
#undef NDEBUG
#include <assert.h>
#include "rtp.h"

void testRTPHeader()
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
	header.Dump();
	extension.Dump();

	//Check
	assert(len);
	assert(extension.hasTransportWideCC);

	//Copy header
	RTPHeader clone(header);
	//Check data
	assert(clone.version==2);
	assert(!clone.padding);
	assert(clone.extension);
	assert(clone.payloadType==98);
	assert(clone.csrcs.size()==0);
	assert(clone.ssrc==0x6232);
	assert(clone.mark);
	assert(clone.timestamp=252617738);
	clone.Dump();
}

void testBye()
{
	BYTE aux[1024];
	BYTE data[] = { 
	0x83,0xcb,0x00,0x06,
	0x01,0x63,0xd0,0xfe,
	0x59,0x97,0xef,0x7b,
	0x0a,0x7d,0x57,0x0e,
	0x0a,0x74,0x65,0x72,
	0x6d,0x69,0x6e,0x61,
	0x74,0x65,0x64,0x00 };

	DWORD size = sizeof(data);

	//Parse it
	auto rtcp = RTCPCompoundPacket::Parse(data,size);
	rtcp->Dump();

	//Serialize
	DWORD len = rtcp->Serialize(aux,1024);
	Dump(data,size);
	Dump(aux,len);

	auto cloned = RTCPCompoundPacket::Parse(aux,len);
	cloned->Dump();
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
	auto rtcp = RTCPCompoundPacket::Parse(msg,sizeof(msg));
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
	auto rtcp = RTCPCompoundPacket::Create();

	//Create NACK
	auto nack = rtcp->CreatePacket<RTCPRTPFeedback>(RTCPRTPFeedback::NACK,2329392274,24891);

	//Add 
	nack->CreateField<RTCPRTPFeedback::NACKField>(4144,(WORD)0x0300);

	//Add to packet
	rtcp->Dump();

	//Serialize it
	DWORD len = rtcp->Serialize(data,1024);

	Dump(data,len);

	assert(RTCPCompoundPacket::IsRTCP(data,len));
	assert(len==sizeof(msg));
	assert(memcmp(msg,data,len)==0);

	//parse it back
	auto cloned = RTCPCompoundPacket::Parse(data,len);

	assert(cloned);

	//Serialize it again
	len = rtcp->Serialize(data,1024);

	Dump(data,len);

	assert(RTCPCompoundPacket::IsRTCP(data,len));
	assert(len==sizeof(msg));
	assert(memcmp(msg,data,len)==0);

	//Dump it
	cloned->Dump();
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
	auto rtcp = RTCPCompoundPacket::Parse(data,size);
	rtcp->Dump();

	//Serialize
	DWORD len = rtcp->Serialize(aux,1024);
	Dump(data,size);
	Dump(aux,len);

	auto cloned = RTCPCompoundPacket::Parse(aux,len);
	cloned->Dump();

	BYTE data2[] = {
	0x8f,0xcd,0x00,0x05,
	0x97,0xb3,0x78,0x4f,
	0x00,0x00,0x40,0x4f,

	0x00,0x45,0x00,0x01,
	0x09,0xf9,0x73,0x00,
	0x20,0x01,0x0c,0x00};

	DWORD size2 = sizeof(data2);

	//Parse it
	auto rtcp2 = RTCPCompoundPacket::Parse(data2,size2);
	rtcp2->Dump();


	len = rtcp2->Serialize(aux,1024);
	Dump(data2,sizeof(data));
	Dump(aux,len);
	auto cloned2 = RTCPCompoundPacket::Parse(aux,len);
	cloned2->Dump();

}

void testSDESHeaderExtensions()
{
	constexpr int8_t kPayloadType = 100;
	constexpr uint16_t kSeqNum = 0x1234;
	constexpr uint8_t kSeqNumFirstByte = kSeqNum >> 8;
	constexpr uint8_t kSeqNumSecondByte = kSeqNum & 0xff;
	constexpr uint8_t RTPStreamId = 0x0a;
	constexpr uint8_t MediaStreamId = 0x0b;
	constexpr uint8_t RepairedRTPStreamId = 0x0c;
	constexpr char kStreamId[] = "streamid";
	constexpr char kRepairedStreamId[] = "repairedid";
	constexpr char kMid[] = "mid";
	constexpr uint8_t kPacketWithRsid[] = {
	    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
	    0x65, 0x43, 0x12, 0x78,
	    0x12, 0x34, 0x56, 0x78,
	    0xbe, 0xde, 0x00, 0x03,
	    0xa7, 's',  't',  'r',
	    'e',  'a',  'm',  'i',
	    'd' , 0x00, 0x00, 0x00};

	constexpr uint8_t kPacketWithMid[] = {
	    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
	    0x65, 0x43, 0x12, 0x78,
	    0x12, 0x34, 0x56, 0x78,
	    0xbe, 0xde, 0x00, 0x01,
	    0xb2, 'm',  'i',  'd'};

	constexpr uint8_t kPacketWithAll[] = {
	    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
	    0x65, 0x43, 0x12, 0x78,
	    0x12, 0x34, 0x56, 0x78,
	    0xbe, 0xde, 0x00, 0x06,
	    0xb2, 'm',  'i',  'd',
	    0xa7, 's',  't',  'r',
	    'e',  'a',  'm',  'i',
	    'd',  0xc9, 'r',  'e',
	    'p',  'a',  'i',  'r',
	    'e',  'd',  'i',  'd'};

	RTPMap	extMap;
	extMap[RTPStreamId] = RTPHeaderExtension::RTPStreamId;
	extMap[MediaStreamId] = RTPHeaderExtension::MediaStreamId;
	extMap[RepairedRTPStreamId] = RTPHeaderExtension::RepairedRTPStreamId;

	//RID
	{
		RTPHeader header;
		RTPHeaderExtension extension;
		//Parse header with rid
		const BYTE* data = kPacketWithRsid;
		DWORD size = sizeof(kPacketWithRsid);
		DWORD len = header.Parse(data,size);

		//Dump
		header.Dump();

		//Check data
		assert(len);
		assert(header.version==2);
		assert(!header.padding);
		assert(header.extension);
		assert(header.payloadType==kPayloadType);
		assert(header.sequenceNumber==kSeqNum);

		//Parse extensions
		len = extension.Parse(extMap,data+len,size-len);

		//Dump
		extension.Dump();

		//Check
		assert(len);
		assert(extension.hasRId);
		assert(strcmp(extension.rid.c_str(),kStreamId)==0);

		//Serialize and ensure it is the same
		BYTE aux[128];
		len = header.Serialize(aux,128);
		len += extension.Serialize(extMap,aux+len,size-len);
		Dump4(data,size);
		Dump4(aux,len);
		assert(len==size);
	}

	//MID
	{
		RTPHeader header;
		RTPHeaderExtension extension;
		//Parse header witn mid
		const BYTE* data = kPacketWithMid;
		DWORD size = sizeof(kPacketWithMid);
		DWORD len = header.Parse(data,size);
		//Dump
		header.Dump();

		//Check data
		assert(len);
		assert(header.version==2);
		assert(!header.padding);
		assert(header.extension);
		assert(header.payloadType==kPayloadType);
		assert(header.sequenceNumber==kSeqNum);

		//Parse extensions
		len = extension.Parse(extMap,data+len,size-len);

		//Dump
		extension.Dump();

		//Check
		assert(len);
		assert(extension.hasMediaStreamId);
		assert(strcmp(extension.mid.c_str(),kMid)==0);

		//Serialize and ensure it is the same
		BYTE aux[128];
		len = header.Serialize(aux,128);
		len += extension.Serialize(extMap,aux+len,size-len);
		Dump4(data,size);
		Dump4(aux,len);
		assert(len==size);
	}

	//ALL
	{
		RTPHeader header;
		RTPHeaderExtension extension;
		//Parse header with all
		const BYTE* data = kPacketWithAll;
		DWORD size = sizeof(kPacketWithAll);
		DWORD len = header.Parse(data,size);

		//Dump
		header.Dump();

		//Check data
		assert(len);
		assert(header.version==2);
		assert(!header.padding);
		assert(header.extension);
		assert(header.payloadType==kPayloadType);
		assert(header.sequenceNumber==kSeqNum);

		//Parse extensions
		len = extension.Parse(extMap,data+len,size-len);

		//Dump
		extension.Dump();

		//Check
		assert(len);
		assert(extension.hasRId);
		assert(strcmp(extension.rid.c_str(),kStreamId)==0);
		assert(extension.hasRepairedId);
		assert(strcmp(extension.repairedId.c_str(),kRepairedStreamId)==0);
		assert(extension.hasMediaStreamId);
		assert(strcmp(extension.mid.c_str(),kMid)==0);

		//Serialize and ensure it is the same
		BYTE aux[128];
		len = header.Serialize(aux,128);
		len += extension.Serialize(extMap,aux+len,size-len);
		Dump4(data,size);
		Dump4(aux,len);
		assert(len==size);
	}

	//MIX - no serialize transport wide cc
	{
		RTPMap	map;
		map[1] = RTPHeaderExtension::AbsoluteSendTime;
		map[2] = RTPHeaderExtension::TimeOffset;
		map[3] = RTPHeaderExtension::MediaStreamId;
		map[4] = RTPHeaderExtension::FrameMarking;

		RTPHeaderExtension extension;
		extension.hasTimeOffset = true;
		extension.hasAbsSentTime = true;
		extension.hasTransportWideCC = true;
		extension.hasFrameMarking = true;
		extension.hasMediaStreamId = true;
		extension.timeOffset = 1800;
		extension.absSentTime = 15069;
		extension.transportSeqNum = 116;
		extension.frameMarks.startOfFrame = 0;
		extension.frameMarks.endOfFrame = 1;
		extension.frameMarks.independent = 0;
		extension.frameMarks.discardable = 0;
		extension.frameMarks.baseLayerSync = 0;
		extension.frameMarks.temporalLayerId = 0;
		extension.frameMarks.layerId = 0;
		extension.frameMarks.tl0PicIdx = 0;
		extension.mid = "sdparta_0";


		//Serialize and ensure it is the same
		BYTE aux[128];
		extension.Dump();
		size_t len = extension.Serialize(map,aux,128);
		Dump4(aux,len);
	}
}

void testTransportWideFeedbackMessage()
{
	DWORD mainSSRC = 1;
	DWORD ssrc = 2;
	DWORD feedbackPacketCount = 1;
	std::map<DWORD,QWORD> packets;
	DWORD lastFeedbackPacketExtSeqNum = 0;
	DWORD feedbackCycles = 0;
/*		
	DWORD i=1;
	packets[i++] = 1000;
	packets[i++] = 2000;
	i++; //lost;
	packets[i++] = 3000;
	packets[i++] = 5000;
	packets[i++] = 4000; //OOO
	packets[i++] = 6000;
	i++; //lost;
	packets[i++] = 7000;
*/
	QWORD initTime = 1515507427620000;
	packets[1912] = 1515507427621937;
	packets[1913] = 1515507427623837;
	packets[1914] = 1515507427675038;
	packets[1915] = 1515507427676752;
	packets[1916] = 1515507427677096;
	packets[1917] = 1515507427678231;
	packets[1918] = 1515507427679458;
	packets[1919] = 1515507427679779;
	packets[1920] = 1515507427681435;
	packets[1921] = 1515507427681649;
	packets[1922] = 1515507427683335;
	packets[1923] = 1515507427689809;
	packets[1924] = 1515507427689901;
	packets[1925] = 1515507427690941;
	packets[1926] = 1515507427691188;
	packets[1927] = 1515507427691561;
	packets[1928] = 1515507427693571;
	packets[1929] = 1515507427710500;
	packets[1930] = 1515507427714770;
	packets[1931] = 1515507427739440;
	packets[1932] = 1515507427784543;

	//Create rtcp transport wide feedback
	auto rtcp = RTCPCompoundPacket::Create();

	//Add to rtcp
	auto feedback = rtcp->CreatePacket<RTCPRTPFeedback>(RTCPRTPFeedback::TransportWideFeedbackMessage,mainSSRC,ssrc);

	//Create trnasport field
	auto field = feedback->CreateField<RTCPRTPFeedback::TransportWideFeedbackMessageField>(feedbackPacketCount++);

	//For each packet stats process it and delete from map
	for (auto it=packets.cbegin();
		  it!=packets.cend();
		  it=packets.erase(it))
	{
		//Get transportSeqNum
		DWORD transportSeqNum = it->first;
		//Get relative time
		QWORD time = it->second - initTime;

		//Check if we have a sequence wrap
		if (transportSeqNum<0x0FFF && (lastFeedbackPacketExtSeqNum & 0xFFFF)>0xF000)
			//Increase cycles
			feedbackCycles++;

		//Get extended value
		DWORD transportExtSeqNum = feedbackCycles<<16 | transportSeqNum;

		//if not first and not out of order
		if (lastFeedbackPacketExtSeqNum && transportExtSeqNum>lastFeedbackPacketExtSeqNum)
			//For each lost
			for (DWORD i = lastFeedbackPacketExtSeqNum+1; i<transportExtSeqNum; ++i)
				//Add it
				field->packets.insert(std::make_pair(i,0));
		//Store last
		lastFeedbackPacketExtSeqNum = transportExtSeqNum;

		//Add this one
		field->packets.insert(std::make_pair(transportSeqNum,time));

	}

	rtcp->Dump();

	//Get sizze
	DWORD size = rtcp->GetSize();

	//Create aux buffer for serializing it
	BYTE* data = (BYTE*)malloc(size);

	//Seriaize
	DWORD len = rtcp->Serialize(data,size);

	assert(len==size);
	Dump4(data,len);

	//Parse it
	auto parsed = RTCPCompoundPacket::Parse(data,len);

	parsed->Dump();

	auto a = rtcp->GetPacket<RTCPRTPFeedback>(0)->GetField<RTCPRTPFeedback::TransportWideFeedbackMessageField>(0)->packets;
	auto b = parsed->GetPacket<RTCPRTPFeedback>(0)->GetField<RTCPRTPFeedback::TransportWideFeedbackMessageField>(0)->packets;

	//Calucalte diferences
	for (auto orig=a.begin(), mod=b.begin(); orig!=a.end(), mod!=b.end(); ++orig, ++mod)
		//Log difference
		Log("-diff %d\n",orig->second-mod->second);

	free(data);
}
void testTransportWideFeedbackMessageParser()
{
	{
		constexpr uint8_t rtcp[] = {
			0x8F, 0xCD, 0x00, 0x07, 0xAA, 0x7D, 0x9D, 0x28,
			0xAA, 0x7D, 0x9D, 0x28, 0x00, 0x02, 0x00, 0x08,
			0x71, 0x66, 0xD7, 0x00, 0x20, 0x08, 0x1C, 0x20,
			0x08, 0x10, 0x20, 0x10, 0x00, 0x14, 0x00, 0x00	
		};

		//Parse it
		auto parsed = RTCPCompoundPacket::Parse(rtcp,sizeof(rtcp));

		if (parsed)
			parsed->Dump();
	}

	{
		constexpr uint8_t rtcp[] = {
			0x8F, 0xCD, 0x00, 0x06, 0xDB, 0x17, 0xE1, 0x2C,
			0xDB, 0x17, 0xE1, 0x2C, 0x00, 0x0A, 0x00, 0x05,
			0x71, 0x66, 0xD7, 0x01, 0x20, 0x05, 0x14, 0x00,
			0x08, 0x00, 0xA8, 0x00
		};
		//Parse it
		auto parsed = RTCPCompoundPacket::Parse(rtcp,sizeof(rtcp));

		if (parsed)
			parsed->Dump();
	}
}

void testRTPPacket()
{

	constexpr int8_t kPayloadType = 100;
	constexpr uint32_t kSsrc = 0x12345678;
	constexpr uint16_t kSeqNum = 0x1234;
	constexpr uint8_t kSeqNumFirstByte = kSeqNum >> 8;
	constexpr uint8_t kSeqNumSecondByte = kSeqNum & 0xff;
	constexpr uint32_t kTimestamp = 0x65431278;
	constexpr uint8_t kTransmissionOffsetExtensionId = 1;
	constexpr uint8_t kAudioLevelExtensionId = 9;
	constexpr uint8_t kRtpStreamIdExtensionId = 0xa;
	constexpr uint8_t kRtpMidExtensionId = 0xb;
	constexpr uint8_t kVideoTimingExtensionId = 0xc;
	constexpr uint8_t kTwoByteExtensionId = 0xf0;
	constexpr uint8_t kTwoByteExtensionMId = 0xf0;
	constexpr int32_t kTimeOffset = 0x56ce;
	constexpr bool kVoiceActive = true;
	constexpr uint8_t kAudioLevel = 0x5a;
	constexpr char kStreamId[] = "streamid";
	constexpr char kMid[] = "mid";
	constexpr char kLongMid[] = "extra-long string to test two-byte header";
	constexpr size_t kMaxPaddingSize = 224u;

	RTPMap	rtpMap;
	RTPMap	extMap;
	extMap[kTransmissionOffsetExtensionId]	= RTPHeaderExtension::TimeOffset;
	extMap[kAudioLevelExtensionId]		= RTPHeaderExtension::SSRCAudioLevel;
	extMap[kRtpStreamIdExtensionId]		= RTPHeaderExtension::RTPStreamId;
	extMap[kRtpMidExtensionId]		= RTPHeaderExtension::MediaStreamId;
	extMap[kTwoByteExtensionMId]		= RTPHeaderExtension::MediaStreamId;

	// clang-format off
	constexpr uint8_t kMinimumPacket[] = {
		0x80, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78
	};
	RTPPacket::Parse(kMinimumPacket, sizeof(kMinimumPacket), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithTO[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0xbe, 0xde, 0x00, 0x01,
		0x12, 0x00, 0x56, 0xce
	};
	RTPPacket::Parse(kPacketWithTO, sizeof(kPacketWithTO), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithTOAndAL[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0xbe, 0xde, 0x00, 0x02,
		0x12, 0x00, 0x56, 0xce,
		0x90, 0x80 | kAudioLevel, 0x00, 0x00
	};
	RTPPacket::Parse(kPacketWithTOAndAL, sizeof(kPacketWithTOAndAL), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithTwoByteExtensionIdLast[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0x10, 0x00, 0x00, 0x04,
		0x01, 0x03, 0x00, 0x56,
		0xce, 0x09, 0x01, 0x80 | kAudioLevel,
		kTwoByteExtensionId, 0x03, 0x00, 0x30, // => 0x00 0x30 0x22
		0x22, 0x00, 0x00, 0x00
	}; // => Playout delay.min_ms = 3*10
	// => Playout delay.max_ms = 34*10
	RTPPacket::Parse(kPacketWithTwoByteExtensionIdLast, sizeof(kPacketWithTwoByteExtensionIdLast), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithTwoByteExtensionIdFirst[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0x10, 0x00, 0x00, 0x04,
		kTwoByteExtensionId, 0x03, 0x00, 0x30, // => 0x00 0x30 0x22
		0x22, 0x01, 0x03, 0x00, // => Playout delay.min_ms = 3*10
		0x56, 0xce, 0x09, 0x01, // => Playout delay.max_ms = 34*10
		0x80 | kAudioLevel, 0x00, 0x00, 0x00
	};
	RTPPacket::Parse(kPacketWithTwoByteExtensionIdFirst, sizeof(kPacketWithTwoByteExtensionIdFirst), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithTOAndALInvalidPadding[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0xbe, 0xde, 0x00, 0x03,
		0x12, 0x00, 0x56, 0xce,
		0x00, 0x02, 0x00, 0x00, // 0x02 is invalid padding, parsing should stop.
		0x90, 0x80 | kAudioLevel, 0x00, 0x00
	};
	RTPPacket::Parse(kPacketWithTOAndALInvalidPadding, sizeof(kPacketWithTOAndALInvalidPadding), rtpMap, extMap);

	constexpr uint8_t kPacketWithTOAndALReservedExtensionId[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0xbe, 0xde, 0x00, 0x03,
		0x12, 0x00, 0x56, 0xce,
		0x00, 0xF0, 0x00, 0x00, // F is a reserved id, parsing should stop.
		0x90, 0x80 | kAudioLevel, 0x00, 0x00
	};
	RTPPacket::Parse(kPacketWithTOAndALReservedExtensionId, sizeof(kPacketWithTOAndALReservedExtensionId), rtpMap, extMap )->Dump();

	constexpr uint8_t kPacketWithRsid[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0xbe, 0xde, 0x00, 0x03,
		0xa7, 's', 't', 'r',
		'e', 'a', 'm', 'i',
		'd', 0x00, 0x00, 0x00
	};
	RTPPacket::Parse(kPacketWithRsid, sizeof(kPacketWithRsid), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithMid[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0xbe, 0xde, 0x00, 0x01,
		0xb2, 'm', 'i', 'd'
	};
	RTPPacket::Parse(kPacketWithMid, sizeof(kPacketWithMid), rtpMap, extMap)->Dump();

	constexpr uint32_t kCsrcs[] = {0x34567890, 0x32435465};
	constexpr uint8_t kPayload[] = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
	constexpr uint8_t kPacketPaddingSize = 8;
	constexpr uint8_t kPacket[] = {
		0xb2, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0x34, 0x56, 0x78, 0x90,
		0x32, 0x43, 0x54, 0x65,
		0xbe, 0xde, 0x00, 0x01,
		0x12, 0x00, 0x56, 0xce,
		'p', 'a', 'y', 'l', 'o', 'a', 'd',
		'p', 'a', 'd', 'd', 'i', 'n', 'g', kPacketPaddingSize
	};
	RTPPacket::Parse(kPacket, sizeof(kPacket), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithTwoByteHeaderExtension[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0x10, 0x00, 0x00, 0x02, // Two-byte header extension profile id + length.
		kTwoByteExtensionId, 0x03, 0x00, 0x56,
		0xce, 0x00, 0x00, 0x00
	};
	RTPPacket::Parse(kPacketWithTwoByteHeaderExtension, sizeof(kPacketWithTwoByteHeaderExtension), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithLongTwoByteHeaderExtension[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0x10, 0x00, 0x00, 0x0B, // Two-byte header extension profile id + length.
		kTwoByteExtensionMId, 0x29, 'e', 'x',
		't', 'r', 'a', '-', 'l', 'o', 'n', 'g',
		' ', 's', 't', 'r', 'i', 'n', 'g', ' ',
		't', 'o', ' ', 't', 'e', 's', 't', ' ',
		't', 'w', 'o', '-', 'b', 'y', 't', 'e',
		' ', 'h', 'e', 'a', 'd', 'e', 'r', 0x00
	};
	RTPPacket::Parse(kPacketWithLongTwoByteHeaderExtension, sizeof(kPacketWithLongTwoByteHeaderExtension), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithTwoByteHeaderExtensionWithPadding[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78,
		0x12, 0x34, 0x56, 0x78,
		0x10, 0x00, 0x00, 0x03, // Two-byte header extension profile id + length.
		kTwoByteExtensionId, 0x03, 0x00, 0x56,
		0xce, 0x00, 0x00, 0x00, // Three padding bytes.
		kAudioLevelExtensionId, 0x01, 0x80 | kAudioLevel, 0x00
	};
	RTPPacket::Parse(kPacketWithTwoByteHeaderExtensionWithPadding, sizeof(kPacketWithTwoByteHeaderExtensionWithPadding), rtpMap, extMap)->Dump();

	constexpr uint8_t kPacketWithInvalidExtension[] = {
		0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
		0x65, 0x43, 0x12, 0x78, // kTimestamp.
		0x12, 0x34, 0x56, 0x78, // kSSrc.
		0xbe, 0xde, 0x00, 0x02, // Extension block of size 2 x 32bit words.
		(kTransmissionOffsetExtensionId << 4) | 6, // (6+1)-byte extension, but
		'e', 'x', 't', // Transmission Offset
		'd', 'a', 't', 'a', // expected to be 3-bytes.
		'p', 'a', 'y', 'l', 'o', 'a', 'd'
	};
	RTPPacket::Parse(kPacketWithInvalidExtension, sizeof(kPacketWithInvalidExtension), rtpMap, extMap)->Dump();
}

int main()
{
	Logger::EnableDebug(true);
	Logger::EnableUltraDebug(true);
	testRTPHeader();
	testRTPPacket();
	testSenderReport();
	testNack();
	testSDESHeaderExtensions();
	testTransportField();
	testTransportWideFeedbackMessage();
	testTransportWideFeedbackMessageParser();
	testBye();
	
	return 0;
}
	
