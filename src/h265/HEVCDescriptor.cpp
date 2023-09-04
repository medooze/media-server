/* 
 * File:   HEVCDescriptor.cpp
 * Author: Zita
 * 
 * Created on 21 June 2023, 12:21
 */

#include "log.h"
#include "HEVCDescriptor.h"
#include "tools.h"
#include <string.h>
#include <stdlib.h>
#include <vector>

HEVCDescriptor::HEVCDescriptor()
{
	//Empty
}

bool HEVCDescriptor::Parse(const BYTE* buffer,DWORD bufferLen)
{
	//Check size
	if (bufferLen < MediaParameterSize)
	{
		//Nothing
		Error("HEVCDescriptot is too short!\n");
		return false;
	}

	// read pointer
	DWORD pos = 0;

	//Get data
	configurationVersion 			= buffer[pos++];

	profileIndication 				= buffer[pos++];
	generalProfileSpace = profileIndication >> 6;
	if (generalProfileSpace > 3)
	{
		Error("-H265: general_profile_space (%d) is too large!\n", generalProfileSpace);
		return false;
	}
	generalTierFlag = (profileIndication >> 5) & 0x1;
	generalProfileIdc = profileIndication & 0x1F;

	//DWORD generalProfileCompatibilityFlags
	generalProfileCompatibilityFlags = 0;
	//for (size_t i = 0; i < sizeof(profileCompatibilityIndication); ++i)
	//{
	//	const BYTE bitOffset = i*8;
	//	profileCompatibilityIndication += ((DWORD)(buffer[pos++]) << bitOffset);
	//}
	generalProfileCompatibilityFlags = get4(buffer, pos);
	pos += 4;
	for (size_t i = 0; i < generalConstraintIndicatorFlags.size(); ++i)
	{
		generalConstraintIndicatorFlags[i] = buffer[pos++];
	}
	generalLevelIdc = buffer[pos++];
	// skip uninterested fields
	pos += paddingBytes.size();
	NALUnitLengthSizeMinus1 = buffer[pos++] & 0x3;
	if (NALUnitLengthSizeMinus1 == 2)
	{
		Error("-H265: Invalid NALU length size\n");
		return false;
	}
	// numOfArray
	pos++;
	if (pos != MediaParameterSize)
	{
		Error(" -H265: HEVCDescriptoer parsing error! pos: %d, MediaParmeterSize: %d\n", pos, MediaParameterSize);
		return false;
	}

	//Clean all parameter sets
	ClearVideoParameterSets();
	ClearSequenceParameterSets();
	ClearPictureParameterSets();

	// VPS
	if( pos + 3 > bufferLen )//Check size
	{
		Error("-H265: Failed to parse VPS count from descriptor! pos: %d, bufferLen: %d\n", pos, bufferLen);
		return false;
	}
	if (buffer[pos++] != HEVC_RTP_NALU_Type::VPS)
	{
		Error("-H265: Failed to parse VPS nal unit type. pos: %d, bufferLen: %d\n", pos, bufferLen);
		return false;
	}
	WORD num = ((WORD)(buffer[pos]) << 8) | buffer[pos+1];
	pos += 2;
	for (WORD i=0;i<num;++i)
	{
		if (pos+2 > bufferLen)//Check size
		{
			Error("-H265: Failed to parse VPS[%d] length from descriptor! pos: %d, bufferLen: %d\n", i, pos, bufferLen);
			return false;
		}
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		if (pos+2+length>bufferLen)//Check size
		{
			Error("-H265: Failed to parse VPS[%d] data from descriptor! pos: %d, bufferLen: %d\n", i, pos, bufferLen);
			return false;
		}
		//Add it
		AddVideoParameterSet(buffer+pos+2,length);
		//Update read pointer
		pos += (length+2);
	}
	// SPS
	if( pos + 3 > bufferLen )//Check size
	{
		Error("-H265: Failed to parse SPS count from descriptor! pos: %d, bufferLen: %d\n", pos, bufferLen);
		return false;
	}
	if (buffer[pos++] != HEVC_RTP_NALU_Type::SPS)
	{
		Error("-H265: Failed to parse SPS nal unit type. pos: %d, bufferLen: %d, data: 0x%02x\n", pos, bufferLen, buffer[pos - 1]);
		return false;
	}
	num = ((WORD)(buffer[pos]) << 8) | buffer[pos+1];
	pos += 2;
	for (DWORD i=0; i<num;i++)
	{
		if (pos+2>bufferLen)//Check size
		{
			Error("-H265: Failed to parse SPS[%d] length from descriptor! pos: %d, bufferLen: %d\n", i, pos, bufferLen);
			return false;
		}
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		
		if (pos+2+length>bufferLen)//Check size
		{
			Error("-H265: Failed to parse SPS[%d] data from descriptor! pos: %d, bufferLen: %d\n", i, pos, bufferLen);
			return false;
		}
		//Add it
		AddSequenceParameterSet(buffer+pos+2,length);
		//Update read pointer
		pos += (length+2);
	}
	// PPS
	if( pos + 3 > bufferLen )//Check size
	{
		Error("-H265: Failed to parse PPS count from descriptor! pos: %d, bufferLen: %d\n", pos, bufferLen);
		return false;
	}
	if (buffer[pos++] != HEVC_RTP_NALU_Type::PPS)
	{
		Error("-H265: Failed to parse PPS nal unit type. pos: %d, bufferLen: %d, data: 0x%02x\n", pos, bufferLen, buffer[pos - 1]);
		return false;
	}
	num = ((WORD)(buffer[pos]) << 8) | buffer[pos+1];
	pos += 2;
	for (WORD i=0; i<num; i++)
	{
		if (pos+2>bufferLen)//Check size
		{
			Error("-H265: Failed to parse PPS[%d] length from descriptor! pos: %d, bufferLen: %d\n", i, pos, bufferLen);
			return false;
		}
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		
		if (pos+2+length>bufferLen)//Check size
		{
			Error("-H265: Failed to parse PPS[%d] data from descriptor! pos: %d, bufferLen: %d\n", i, pos, bufferLen);
			return false;
		}
		//Add it
		AddPictureParameterSet(buffer+pos+2,length);
		//Update read pointer
		pos+=length+2;
	}

	if (pos != bufferLen)
	{
		Error("-H265: Error on parsing HEVCDesciptor, with final pos: %d, but bufferLen: %d!\n", pos, bufferLen);
	}
	return true;
}

HEVCDescriptor::~HEVCDescriptor()
{
	//REmove VPS
	ClearVideoParameterSets();
	//REmove PPS
	ClearSequenceParameterSets();
	//Remove SPS
	ClearPictureParameterSets();
}

void HEVCDescriptor::AddVideoParameterSet(const BYTE *data,DWORD size)
{
	//Increase number of vps
	numOfVideoParameterSets++;
	//Create data
	BYTE* vps = (BYTE*)malloc(size);
	//Copy
	memcpy(vps,data,size);
	//Add size
	vpsSizes.push_back(size);
	//Add data
	vpsData.push_back(vps);
	//INcrease size
	vpsTotalSizes+=size;
}

void HEVCDescriptor::AddSequenceParameterSet(const BYTE *data,DWORD size)
{
	//Increase number of pps
	numOfSequenceParameterSets++;
	//Create data
	BYTE* sps = (BYTE*)malloc(size);
	//Copy
	memcpy(sps,data,size);
	//Add size
	spsSizes.push_back(size);
	//Add data
	spsData.push_back(sps);
	//INcrease size
	spsTotalSizes+=size;
}

void HEVCDescriptor::AddPictureParameterSet(const BYTE *data,DWORD size)
{
	//Increase number of pps
	numOfPictureParameterSets++;
	//Create data
	BYTE* pps = (BYTE*)malloc(size);
	//Copy
	memcpy(pps,data,size);
	//Add size
	ppsSizes.push_back(size);
	//Add data
	ppsData.push_back(pps);
	//INcrease size
	ppsTotalSizes+=size;
}

void HEVCDescriptor::AddParametersFromFrame(const BYTE *data,DWORD size)
{
	//Chop into NALs
	while(size>4)
	{
		//Get nal size
		DWORD nalSize =  get4(data,0);
		//Get NAL start
		const BYTE* nal = data+4;
		//Depending on the type
		switch((nal[0] & 0b0111'1110) >> 1)
		{
			case HEVC_RTP_NALU_Type::VPS:
				AddVideoParameterSet(nal, nalSize);
				break;
			case HEVC_RTP_NALU_Type::PPS:
				//Append
				AddPictureParameterSet(nal,nalSize);
				break;
			case HEVC_RTP_NALU_Type::SPS:
				//Append
				AddSequenceParameterSet(nal,nalSize);
				break;
		}
		//Skip it
		data+=4+nalSize;
		size-=4+nalSize;
	}
}

void  HEVCDescriptor::ClearVideoParameterSets()
{
	//Free vps memory
	while(!vpsData.empty())
	{
		//Free memory
		free(vpsData.back());
		//remove
		vpsData.pop_back();
	}
	//Cleare sizes
	vpsSizes.clear();
	//Clear sizes
	vpsTotalSizes = 0;
	//No params
	numOfVideoParameterSets = 0;
}

void  HEVCDescriptor::ClearSequenceParameterSets()
{
	//Free sps memory
	while(!spsData.empty())
	{
		//Free memory
		free(spsData.back());
		//remove
		spsData.pop_back();
	}
	//Cleare sizes
	spsSizes.clear();
	//Clear sizes
	spsTotalSizes = 0;
	//No params
	numOfSequenceParameterSets = 0;
}

void  HEVCDescriptor::ClearPictureParameterSets()
{
	//Free pps memory
	while(!ppsData.empty())
	{
		//Free memory
		free(ppsData.back());
		//remove
		ppsData.pop_back();
	}
	//Cleare sizes
	ppsSizes.clear();
	//Cleare total size
	ppsTotalSizes = 0;
	//No params
	numOfPictureParameterSets = 0;
}

DWORD HEVCDescriptor::Serialize(BYTE* buffer,DWORD bufferLength) const
{
	//write pointer
	DWORD pos = 0;
	//Check size
	if (bufferLength < GetSize())
		//Nothing
		return -1;

	buffer[pos++] = configurationVersion;
	buffer[pos++] = profileIndication;
	//DWORD profileCompatibilityIndication
	set4(buffer, pos, generalProfileCompatibilityFlags);
	pos += 4;
	memcpy(buffer+pos, generalConstraintIndicatorFlags.data(), generalConstraintIndicatorFlags.size());
	pos += generalConstraintIndicatorFlags.size();
	buffer[pos++] = generalLevelIdc;
	memcpy(buffer+pos, paddingBytes.data(), paddingBytes.size());
	pos += paddingBytes.size();
	buffer[pos++] = NALUnitLengthSizeMinus1 | 0b1111'1100; //bit(2) constantFrameRate + bit(3) numTemporalLayers + bit(1) temporalIdNested; + unsigned int(2) lengthSizeMinusOne;
	buffer[pos++] = numOfArrays;
	if (pos != MediaParameterSize)
	{
		Error("-H265: HEVCDescriptoer serialization error! pos: %d, MediaParameterSize: %d \n", pos, MediaParameterSize);
		return -1;
	}

	// VPS
	buffer[pos++] = HEVC_RTP_NALU_Type::VPS;  // bit(1) array_completeness + unsigned int(1) reserved = 0; unsigned int(6) NAL_unit_type;
	// numOfVideoParameterSets: 2 BYTE
	buffer[pos++] = numOfVideoParameterSets >> 8;
	buffer[pos++] = numOfVideoParameterSets & 0xff;
	//Debug("-H265: start to serialize VPS [pos: %d, VPS count: %d]\n", pos, numOfVideoParameterSets);
	for (BYTE i=0;i<numOfVideoParameterSets;i++)
	{
		//Get length
		WORD length = vpsSizes[i];
		//Set len: 2 Bytes
		buffer[pos]	= length >> 8;
		buffer[pos+1]	= length ;
		//Copy sps: length Bytes
		memcpy(buffer+pos+2,vpsData[i],length);
		//Update write pointer
		pos+=length+2;
	}
	// SPS
	buffer[pos++] = HEVC_RTP_NALU_Type::SPS;  // bit(1) array_completeness + unsigned int(1) reserved = 0; unsigned int(6) NAL_unit_type;
	// numOfSequenceParameterSets: 2 BYTE
	buffer[pos++] = numOfSequenceParameterSets >> 8;
	buffer[pos++] = numOfSequenceParameterSets & 0xff;
	//Debug("-H265: start to serialize SPS [pos: %d, SPS count: %d]\n", pos, numOfSequenceParameterSets);
	for (BYTE i=0;i<numOfSequenceParameterSets;i++)
	{
		//Get length
		WORD length = spsSizes[i];
		//Set len
		buffer[pos]	= length >> 8;
		buffer[pos+1]	= length;
		//Copy sps
		memcpy(buffer+pos+2,spsData[i],length);
		//Update write pointer
		pos+=length+2;
	}
	// PPS
	buffer[pos++] = HEVC_RTP_NALU_Type::PPS;  // bit(1) array_completeness + unsigned int(1) reserved = 0; unsigned int(6) NAL_unit_type;
	// numOfPictureParameterSets: 2 BYTE
	buffer[pos++] = numOfPictureParameterSets >> 8;
	buffer[pos++] = numOfPictureParameterSets & 0xff;
	//Debug("-H265: start to serialize PPS [pos: %d, SPS count: %d]\n", pos, numOfPictureParameterSets);
	for (BYTE i=0;i<numOfPictureParameterSets;i++)
	{
		//Get length
		WORD length = ppsSizes[i];
		//Set len
		buffer[pos]	= length >> 8;
		buffer[pos+1]	= length;
		//Copy pps
		memcpy(buffer+pos+2,ppsData[i],length);
		//Update write pointer
		pos+=length+2;
	}
	//Finished
	return pos;
}

void HEVCDescriptor::Dump() const
{
	//Get data
	Debug("[HEVCDescriptor\n");
	Debug("\tconfigurationVersion: %d\n",configurationVersion);
	Debug("\tgeneralProfileSpace: %d\n",generalProfileSpace);
	Debug("\tgeneralTierFlag: %d\n",generalTierFlag);
	Debug("\tgeneralProfileIdc: %d\n",generalProfileIdc);
	Debug("\tprofileIndication: 0x%02x\n",profileIndication);
	Debug("\tgeneralprofileCompatibilityIndication: 0x%x\n",generalProfileCompatibilityFlags);
	for (size_t i = 0; i < generalConstraintIndicatorFlags.size(); ++i)
	{
		Debug("\t: generalConstraintIndicatorFlags[%zu]: 0x%02x\n", i, generalConstraintIndicatorFlags[i]);
	}
	Debug("\tgeneralLevelIdc: %d\n", generalLevelIdc);
	Debug("\tNALUnitLengthSizeMinus1: %d\n", NALUnitLengthSizeMinus1);
	Debug("\tnumOfVideoParameterSets: %d\n",numOfVideoParameterSets);
	Debug("\tnumOfSequenceParameterSets: %d\n",numOfSequenceParameterSets);
	Debug("\tnumOfPictureParameterSets: %d\n",numOfPictureParameterSets);
	//Dump VPS
	for (BYTE i=0;i<numOfVideoParameterSets;i++)
	{
		H265VideoParameterSet vps;
		//Decode
		vps.Decode(vpsData[i] + HEVCParams::RTP_NAL_HEADER_SIZE,vpsSizes[i] - HEVCParams::RTP_NAL_HEADER_SIZE);
		//Dump
		vps.Dump();
	}
	//Dump SPS
	for (BYTE i=0;i<numOfSequenceParameterSets;i++)
	{
		H265SeqParameterSet sps;
		//Decode
		sps.Decode(spsData[i] + HEVCParams::RTP_NAL_HEADER_SIZE,spsSizes[i] - HEVCParams::RTP_NAL_HEADER_SIZE);
		//Dump
		sps.Dump();
	}
	//Dump PPS
	for (BYTE i=0;i<numOfPictureParameterSets;i++)
	{
		H265PictureParameterSet pps;
		//Decode
		pps.Decode(ppsData[i] + HEVCParams::RTP_NAL_HEADER_SIZE,ppsSizes[i] - HEVCParams::RTP_NAL_HEADER_SIZE);
		//Dump
		pps.Dump();
	}
	Debug("[HEVCDescriptor/]\n");
}
