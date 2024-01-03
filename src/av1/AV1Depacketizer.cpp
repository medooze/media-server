/* 
 * File:   h264depacketizer.cpp
 * Author: Sergio
 * 
 * Created on 26 de enero de 2012, 9:46
 */

#include "AV1.h"
#include "AV1Depacketizer.h"
#include "media.h"
#include "codecs.h"
#include "rtp.h"
#include "BufferReader.h"
#include "BufferWritter.h"
#include "log.h"

 //
 // OBU syntax:
 //     0 1 2 3 4 5 6 7
 //    +-+-+-+-+-+-+-+-+
 //    |0| type  |X|S|-| (REQUIRED)
 //    +-+-+-+-+-+-+-+-+
 // X: | TID |SID|-|-|-| (OPTIONAL)
 //    +-+-+-+-+-+-+-+-+
 //    |1|             |
 //    +-+ OBU payload |
 // S: |1|             | (OPTIONAL, variable length leb128 encoded)
 //    +-+    size     |
 //    |0|             |
 //    +-+-+-+-+-+-+-+-+
 //    |  OBU payload  |
 //    |     ...       |
constexpr uint8_t ObuTypeBits		 = 0b0'1111'000;
constexpr uint8_t ObuExtensionPresentBit = 0b0'0000'100;
constexpr uint8_t ObuSizePresentBit	 = 0b0'0000'010;



AV1Depacketizer::AV1Depacketizer() : RTPDepacketizer(MediaFrame::Video,VideoCodec::AV1), frame(VideoCodec::AV1,0)
{
	//Generic AV1 config
	AV1CodecConfig config;
	//Set config size
	frame.AllocateCodecConfig(config.GetSize());
	//Serialize
	config.Serialize(frame.GetCodecConfigData(),frame.GetCodecConfigSize());
	//Set clock rate
	frame.SetClockRate(90000);
	//Disable shared buffer
	frame.DisableSharedBuffer();
}

AV1Depacketizer::~AV1Depacketizer()
{

}

void AV1Depacketizer::ResetFrame()
{
	//Reset frame data
	frame.Reset();
	//Generic AV1 config
	AV1CodecConfig config;
	//Set config size
	frame.AllocateCodecConfig(config.GetSize());
	//Serialize
	config.Serialize(frame.GetCodecConfigData(),frame.GetCodecConfigSize());
}

MediaFrame* AV1Depacketizer::AddPacket(const RTPPacket::shared& packet)
{
	//Get timestamp in ms
	auto ts = packet->GetExtTimestamp();
	//Check it is from same packet
	if (frame.GetTimeStamp()!=ts)
		//Reset frame
		ResetFrame();
	//If not timestamp
	if (frame.GetTimeStamp()==(DWORD)-1)
	{
		//Set timestamp
		frame.SetTimestamp(ts);
		//Set clock rate
		frame.SetClockRate(packet->GetClockRate());
		//Set time
		frame.SetTime(packet->GetTime());
		//Set sender time
		frame.SetSenderTime(packet->GetSenderTime());
	}
	//Set SSRC
	frame.SetSSRC(packet->GetSSRC());

	//IF it is the first packet of the layer frame
	if (packet->HasDependencyDestriptor())
	{
		//Get dependency structure
		auto& dependencyDescriptor		= packet->GetDependencyDescriptor();
		auto& templateDependencyStructure	= packet->GetTemplateDependencyStructure();

		//Ensure we have the template
		if (dependencyDescriptor && templateDependencyStructure && dependencyDescriptor->startOfFrame &&
			templateDependencyStructure->ContainsFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId))
		{
			//Get template
			const auto& frameDependencyTemplate = templateDependencyStructure->GetFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId);

			// Set values
			layer.pos = frame.GetLength();
			layer.size = 0;
			layer.info.spatialLayerId = frameDependencyTemplate.spatialLayerId;
			layer.info.temporalLayerId = frameDependencyTemplate.temporalLayerId;

			if (templateDependencyStructure->resolutions.size() > frameDependencyTemplate.spatialLayerId)
			{
				//Set layer info
				layer.width = templateDependencyStructure->resolutions[frameDependencyTemplate.spatialLayerId].width;
				layer.height = templateDependencyStructure->resolutions[frameDependencyTemplate.spatialLayerId].height;
			} else {
				layer.width = 0;
				layer.height = 0;
			}
		}
		
		//Add payload
		AddPayload(packet->GetMediaData(), packet->GetMediaLength());

		//IF it is the first last packet of the layer frame
		if (dependencyDescriptor && dependencyDescriptor->endOfFrame)
		{
			//Set total layer size
			layer.pos = frame.GetLength();
			//Add it
			frame.AddLayerFrame(layer);

		}
	} else {
		//Add payload
		AddPayload(packet->GetMediaData(), packet->GetMediaLength());
	}


	/*
	if (packet->GetMark())
	{
		UltraDebug("-AV1Depacketizer::AddPacket() | Gor frame [intra:%d,size:%d]------------------------------------------\n",frame.IsIntra(),frame.GetLength());

		BufferReader reader(frame.GetData(), frame.GetLength());

		while (reader.GetLeft()>1)
		{
			::Dump(reader.PeekData(), 8);
			//Get obu header
			uint8_t header = reader.Get1();
			uint8_t ext = 0;

			//Get header info
			bool obuExtensionPresent = header & ObuExtensionPresentBit;
			bool obuSizePresent = header & ObuSizePresentBit;

			//If we have the extended info
			if (obuExtensionPresent)
				//Read it
				ext = reader.Get1();

			//If we don' thave obu size
			if (!obuSizePresent)
				return (MediaFrame*)Error("NOOOO\n");

			//Get Length
			auto size = reader.DecodeLev128();
			
			//Skipt it
			reader.Skip(size);

			UltraDebug("-AV1Depacketizer::AddPacket()  | OBU [size:%d,header:0x%x,X:%d,S:%d,ext:%d]\n", size, header, obuExtensionPresent, obuSizePresent);
		}
	}
	*/
	//If it is last return frame
	return packet->GetMark() ? &frame : NULL;
}

MediaFrame* AV1Depacketizer::AddPayload(const BYTE* payload, DWORD len)
{
	//Check length
	if (!len)
		//Exit
		return NULL;
    
	// AV1 format:
	//
	// RTP payload syntax:
	//     0 1 2 3 4 5 6 7
	//    +-+-+-+-+-+-+-+-+
	//    |Z|Y| W |N|-|-|-| (REQUIRED)
	//    +=+=+=+=+=+=+=+=+ (REPEATED W-1 times, or any times if W = 0)
	//    |1|             |
	//    +-+ OBU fragment|
	//    |1|             | (REQUIRED, leb128 encoded)
	//    +-+    size     |
	//    |0|             |
	//    +-+-+-+-+-+-+-+-+
	//    |  OBU fragment |
	//    |     ...       |
	//    +=+=+=+=+=+=+=+=+
	//    |     ...       |
	//    +=+=+=+=+=+=+=+=+ if W > 0, last fragment MUST NOT have size field
	//    |  OBU fragment |
	//    |     ...       |
	//    +=+=+=+=+=+=+=+=+
	BufferReader reader(payload, len);
		
	//Get aggregation header
	BYTE aggregationHeader = reader.Get1();

	//If nothing else
	if (!reader.GetLeft())
	{
		//Error
		UltraDebug("-AV1Depacketizer::AddPayload() | No obu data present in payload");
		return NULL;
	}

	//Read aggregation header info
	bool firstIsFragmented		 =  aggregationHeader & 0b1000'0000u;
	bool lastIsFragmented		 =  aggregationHeader & 0b0100'0000u;
	int  numObus			 = (aggregationHeader & 0b0011'0000u) >> 4;
	bool startsNewCodedVideoSequence =  aggregationHeader & 0b0000'1000u;

	//UltraDebug("-AV1Depacketizer::AddPayload() | new payload [z:%d,y:%d,w:%d,n:%d,payloadlen:%d,framelen:%d]\n", firstIsFragmented, lastIsFragmented, numObus, startsNewCodedVideoSequence, len, frame.GetLength());

	//If is first
	if (!frame.GetLength())
		//calculate if it is an iframe
		frame.SetIntra(startsNewCodedVideoSequence);

	//Number of obus elements in payload
	int num = 1;
	
	//Read the rest of the content
	while (reader.GetLeft())
	{	
		//Get obu length, last obu element length is just the rest of the data available
		uint64_t size = (num!=numObus) ? reader.DecodeLev128() : reader.GetLeft();

		//Is it first or last?
		bool first = (num==1);
		bool last  = (num==numObus) || (numObus==0 && size==reader.GetLeft());

		//UltraDebug("-AV1Depacketizer::AddPayload() | Obu element [num:%d,size:%d,left:%d,first:%d,last:%d]\n", num, size, reader.GetLeft(), first, last);

		//Ensure we have enought size in payload
		if (size>reader.GetLeft())
		{
			//Error
			UltraDebug("-AV1Depacketizer::AddPayload() | not enought data for next obu [left:%llu,obu:%llu]\n", reader.GetLeft(), size);
			return NULL;
		}

		//Get next obu element reader
		BufferReader element = reader.GetReader(size);

		//If it is the first obu element, and it was fragmented
		if (first && firstIsFragmented)
		{
			//Ensure that we had previous data, if not this obu is corrupted
			if (fragment.GetSize())
			{
				//Append the current data to the fragment
				fragment.AppendData<BufferReader>(element);
				//If it also the last element in this fragmet
				if (!last || !lastIsFragmented)
				{
					//We have a complete obu in the fragment
					BufferReader obu(fragment);
					// add to frame
					AddObu(obu);
					//Reset fragment data
					fragment.Reset();
				}
			}
		//If it is the last one and it is fragmented
		} else if (last && lastIsFragmented) {
			//If it is the first in fragment
			if (!first || !firstIsFragmented)
				//Reset fragment data
				fragment.Reset();
			//Append the current data to the fragment
			fragment.AppendData(element);
		//It is a complete obu element
		} else {
			//Add obu to frame
			AddObu(element);
		}

		//One more obu
		num++;
	}
    
	return &frame;
}


void AV1Depacketizer::AddObu(BufferReader& obu)
{
	//Get obu header
	uint8_t header = obu.Get1();
	uint8_t ext = 0;

	//Get header info
	bool obuExtensionPresent = header & ObuExtensionPresentBit;
	bool obuSizePresent	 = header & ObuSizePresentBit;
	bool obuType		 = header & ObuTypeBits >> 3;

	//UltraDebug(">AV1Depacketizer::AddObu() [size:%d,header:0x%x,X:%d,S:%d]\n", obu.GetLeft(), header, obuExtensionPresent, obuSizePresent);

	//If we have the extended info
	if (obuExtensionPresent)
		//Read it
		ext = obu.Get1();

	//If we don' thave obu size
	if (obuSizePresent)
	{
		//Get Length
		auto size = obu.DecodeLev128();
		//Ensure the size is correct
		if (size!=obu.GetLeft())
		{
			UltraDebug("-AV1Depacketizer::AddObu() | Droping obu, size not correct [obu:%llu,lev128:%llu]\n",obu.GetLeft(),size);
			//Skip obu
			return;
		}
	}

	//Override the S bit
	header |= ObuSizePresentBit;

	//Write 1 byte header
	frame.AppendMedia(&header, 1);

	//If we have the extended info
	if (obuExtensionPresent)
		//Write 1 byte header extension
		frame.AppendMedia(&ext, 1);

	//Write the obu size
	uint8_t obuSize[5] = {};
	BufferWritter writter(obuSize, sizeof(obuSize));

	//Encode length
	int len = writter.EncodeLeb128(obu.GetLeft());

	//Write length
	frame.AppendMedia(obuSize,len);

	//If it is a sequence header
	if (obuType == 1)
	{
		SequenceHeaderObu sequenceHeaderObu;

		//Parse it
		if (sequenceHeaderObu.Parse(obu.PeekData(), obu.GetLeft()))
		{
			//Set width and height
			frame.SetWidth(sequenceHeaderObu.max_frame_width_minus_1 + 1);
			frame.SetHeight(sequenceHeaderObu.max_frame_height_minus_1 + 1);
		}
	}

	//Write the rest of the obu
	frame.AppendMedia(obu);
}