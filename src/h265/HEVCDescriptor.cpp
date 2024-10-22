#include <string.h>
#include <stdlib.h>
#include <vector>

#include "log.h"
#include "tools.h"

#include "HEVCDescriptor.h"
#include "BufferReader.h"
#include "BufferWritter.h"


bool HEVCDescriptor::Parse(const BYTE* buffer, DWORD bufferLen)
{

	::Dump(buffer, bufferLen);
	if (bufferLen < MediaParameterSize)
		//Nothing
		return Error("-HEVCDescriptor::Parse() is too short!\n");

	BufferReader reader(buffer, bufferLen);

	//Get data
	configurationVersion	= reader.Get1();
	profileIndication	= reader.Get1();

	generalProfileSpace	= profileIndication >> 6;
	generalTierFlag		= (profileIndication >> 5) & 0x1;
	generalProfileIdc	= profileIndication & 0x1F;

	if (generalProfileSpace > 3)
		return Warning("-HEVCDescriptor::Parse() general_profile_space (%d) is too large!\n", generalProfileSpace);


	generalProfileCompatibilityFlags	= reader.Get4();
	generalConstraintIndicatorFlags		= reader.Get<constraintIndicatorFlagsSize>();
	generalLevelIdc				= reader.Get1();
	paddingBytes				= reader.Get<paddingBytesSize>();
	NALUnitLengthSizeMinus1			= reader.Get1() & 0x3;
	if (NALUnitLengthSizeMinus1 == 2)
		return Warning("HEVCDescriptor::Parse() Invalid NALU length size\n");
	numOfArrays = reader.Get1();

	//Clean all parameter sets
	ClearVideoParameterSets();
	ClearSequenceParameterSets();
	ClearPictureParameterSets();

	for (size_t i = 0; i< numOfArrays; ++i)
	{
		if (!reader.Assert(3))
			return Warning("HEVCDescriptor::Parse() Failed to parse count from descriptor!\n");

		BYTE type = reader.Get1() & 0b0011'1111;
		WORD num = reader.Get2();

		for (WORD i = 0; i < num; ++i)
		{
			if (!reader.Assert(2))
				return Warning("HEVCDescriptor::Parse() Failed to parse [%d] length from descriptor!d\n", i);

			WORD length = reader.Get2();
			if (!reader.Assert(length))
				return Warning("HEVCDescriptor::Parse() Failed to parse [%d] not enought data length:%d left:%d!\n", i, reader.GetLeft(), length);

			Buffer nal = reader.GetBuffer(length);

			//Check NAL type
			switch (type)
			{
				case HEVC_RTP_NALU_Type::VPS:
					AddVideoParameterSet(std::move(nal));
					break;
				case HEVC_RTP_NALU_Type::SPS:
					AddSequenceParameterSet(std::move(nal));
					break;
				case HEVC_RTP_NALU_Type::PPS:
					AddPictureParameterSet(std::move(nal));
					break;
				default:
					Warning("HEVCDescriptor::Parse() Unexpected type:%x\n", type);
			}
		}
	}
	return true;
}

void HEVCDescriptor::AddVideoParameterSet(Buffer&& buffer)
{
	//Increase number of vps
	numOfVideoParameterSets++;
	vpsTotalSizes += buffer.GetSize();
	//Add data
	vpsData.push_back(std::move(buffer));
	
}

void HEVCDescriptor::AddVideoParameterSet(const BYTE* data, DWORD size)
{
	AddVideoParameterSet(std::move(Buffer(data, size)));
}

void HEVCDescriptor::AddSequenceParameterSet(Buffer&& buffer)
{
	//Increase number of sps
	numOfSequenceParameterSets++;
	spsTotalSizes += buffer.GetSize();
	//Add data
	spsData.push_back(std::move(buffer));
}

void HEVCDescriptor::AddSequenceParameterSet(const BYTE* data, DWORD size)
{
	AddSequenceParameterSet(std::move(Buffer(data, size)));
}

void HEVCDescriptor::AddPictureParameterSet(Buffer&& buffer)
{
	//Increase number of pps
	numOfPictureParameterSets++;
	ppsTotalSizes += buffer.GetSize();
	//Add data
	ppsData.push_back(std::move(buffer));
}

void HEVCDescriptor::AddPictureParameterSet(const BYTE* data, DWORD size)
{
	AddPictureParameterSet(std::move(Buffer(data, size)));
}

void HEVCDescriptor::AddParametersFromFrame(const BYTE* data, DWORD size)
{
	//Chop into NALs
	while (size > 4)
	{
		//Get nal size
		DWORD nalSize = get4(data, 0);
		//Get NAL start
		const BYTE* nal = data + 4;
		//Depending on the type
		switch ((nal[0] & 0b0111'1110) >> 1)
		{
			case HEVC_RTP_NALU_Type::VPS:
				AddVideoParameterSet(nal, nalSize);
				break;
			case HEVC_RTP_NALU_Type::PPS:
				//Append
				AddPictureParameterSet(nal, nalSize);
				break;
			case HEVC_RTP_NALU_Type::SPS:
				//Append
				AddSequenceParameterSet(nal, nalSize);
				break;
		}
		//Skip it
		data += 4 + nalSize;
		size -= 4 + nalSize;
	}
}

void  HEVCDescriptor::ClearVideoParameterSets()
{
	//Free vps data
	vpsData.clear();
	//Clear sizes
	vpsTotalSizes = 0;
	//No params
	numOfVideoParameterSets = 0;
}

void  HEVCDescriptor::ClearSequenceParameterSets()
{
	//Free sps data
	spsData.clear();
	//Clear sizes
	spsTotalSizes = 0;
	//No params
	numOfSequenceParameterSets = 0;
}

void  HEVCDescriptor::ClearPictureParameterSets()
{
	//Free sps data
	ppsData.clear();
	//Cleare total size
	ppsTotalSizes = 0;
	//No params
	numOfPictureParameterSets = 0;
}

DWORD HEVCDescriptor::Serialize(BYTE* buffer, DWORD bufferLength) const
{
	if (bufferLength < GetSize())
		//Nothing
		return -1;

	BufferWritter writter(buffer, bufferLength);

	writter.Set1(configurationVersion);
	writter.Set1(profileIndication);
	writter.Set4(generalProfileCompatibilityFlags);
	writter.Set(generalConstraintIndicatorFlags);
	writter.Set1(generalLevelIdc);
	writter.Set(paddingBytes);
	writter.Set1(NALUnitLengthSizeMinus1 | 0b1111'1100); //bit(2) constantFrameRate + bit(3) numTemporalLayers + bit(1) temporalIdNested; + unsigned int(2) lengthSizeMinusOne;
	writter.Set1(numOfArrays);

	// VPS
	writter.Set1(HEVC_RTP_NALU_Type::VPS);  // bit(1) array_completeness + unsigned int(1) reserved = 0; unsigned int(6) NAL_unit_type;
	writter.Set2(numOfVideoParameterSets);
	for (auto& data : vpsData)
	{
		writter.Set2(data.GetSize());
		writter.Set(data);
	}
	// SPS
	writter.Set1(HEVC_RTP_NALU_Type::SPS);  // bit(1) array_completeness + unsigned int(1) reserved = 0; unsigned int(6) NAL_unit_type;
	writter.Set2(numOfSequenceParameterSets);
	for (auto& data : spsData)
	{
		writter.Set2(data.GetSize());
		writter.Set(data);
	}

	// PPS
	writter.Set1(HEVC_RTP_NALU_Type::PPS);  // bit(1) array_completeness + unsigned int(1) reserved = 0; unsigned int(6) NAL_unit_type;
	writter.Set2(numOfPictureParameterSets);
	for (auto& data : ppsData)
	{
		writter.Set2(data.GetSize());
		writter.Set(data);
	}

	//Finished
	return writter.GetLength();
}

void HEVCDescriptor::Dump() const
{
	//Get data
	Debug("[HEVCDescriptor\n");
	Debug("\tconfigurationVersion: %d\n", configurationVersion);
	Debug("\tgeneralProfileSpace: %d\n", generalProfileSpace);
	Debug("\tgeneralTierFlag: %d\n", generalTierFlag);
	Debug("\tgeneralProfileIdc: %d\n", generalProfileIdc);
	Debug("\tprofileIndication: 0x%02x\n", profileIndication);
	Debug("\tgeneralprofileCompatibilityIndication: 0x%x\n", generalProfileCompatibilityFlags);
	for (size_t i = 0; i < generalConstraintIndicatorFlags.size(); ++i)
	{
		Debug("\t: generalConstraintIndicatorFlags[%zu]: 0x%02x\n", i, generalConstraintIndicatorFlags[i]);
	}
	Debug("\tgeneralLevelIdc: %d\n", generalLevelIdc);
	Debug("\tNALUnitLengthSizeMinus1: %d\n", NALUnitLengthSizeMinus1);
	Debug("\tnumOfVideoParameterSets: %d\n", numOfVideoParameterSets);
	Debug("\tnumOfSequenceParameterSets: %d\n", numOfSequenceParameterSets);
	Debug("\tnumOfPictureParameterSets: %d\n", numOfPictureParameterSets);
	//Dump VPS
	for (BYTE i = 0; i < numOfVideoParameterSets; i++)
	{
		H265VideoParameterSet vps;
		//Decode
		vps.Decode(vpsData[i].GetData() + HEVCParams::RTP_NAL_HEADER_SIZE, vpsData[i].GetSize() - HEVCParams::RTP_NAL_HEADER_SIZE);
		//Dump
		vps.Dump();
	}
	//Dump SPS
	for (BYTE i = 0; i < numOfSequenceParameterSets; i++)
	{
		H265SeqParameterSet sps;
		//Decode
		sps.Decode(spsData[i].GetData() + HEVCParams::RTP_NAL_HEADER_SIZE, spsData[i].GetSize() - HEVCParams::RTP_NAL_HEADER_SIZE);
		//Dump
		sps.Dump();
	}
	//Dump PPS
	for (BYTE i = 0; i < numOfPictureParameterSets; i++)
	{
		H265PictureParameterSet pps;
		//Decode
		pps.Decode(ppsData[i].GetData() + HEVCParams::RTP_NAL_HEADER_SIZE, ppsData[i].GetSize() - HEVCParams::RTP_NAL_HEADER_SIZE);
		//Dump
		pps.Dump();
	}
	Debug("[HEVCDescriptor/]\n");
}
