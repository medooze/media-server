/* 
 * File:   HEVCDescriptor.cpp
 * Author: Sergio
 * 
 * Created on 21 de junio de 2011, 12:21
 */
#include "log.h"
#include "HEVCDescriptor.h"
#include "tools.h"
#include "h265.h"
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
	profileSpace  					= buffer[pos++];
	tierFlag   						= buffer[pos++];
	profileIndication 				= buffer[pos++];
	//DWORD profileCompatibilityIndication
	profileCompatibilityIndication = 0;
	for (size_t i = 0; i < sizeof(profileCompatibilityIndication); ++i)
	{
		const BYTE bitOffset = i*8;
		profileCompatibilityIndication += ((DWORD)(buffer[pos++]) << bitOffset);
	}
	//WORD interopConstraints
	interopConstraints				= buffer[pos] + ((WORD)(buffer[pos + 1]) << 8);
	pos += 2;
	levelIndication   				= buffer[pos++];
	NALUnitLength   				= buffer[pos++] & 0x03;	// last 2 bits

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
	if( pos + 1 > bufferLen )//Check size
	{
		Error("-H265: Failed to parse VPS count from descriptor! pos: %d, bufferLen: %d\n", pos, bufferLen);
		return false;
	}
	BYTE num = buffer[pos++];
	for (BYTE i=0;i<num;i++)
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
		pos+=length+2;
	}
	// SPS
	if (pos+1>bufferLen)//Check size
	{
		Error("-H265: Failed to parse SPS count from descriptor! pos: %d, bufferLen: %d\n", pos, bufferLen);
		return false;
	}
	num = buffer[pos++];
	for (DWORD i=0;i<num;i++)
	{
		//Get nul_layer_id	
		if (pos+1>bufferLen)//Check size
		{
			Error("-H265: Failed to parse SPS[%d] nul_layer_id from descriptor! pos: %d, bufferLen: %d\n", i, pos, bufferLen);
			return false;
		}
		BYTE nul_layer_id = buffer[pos++];
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
		AddSequenceParameterSet(buffer+pos+2,length, nul_layer_id);
		//Update read pointer
		pos+=length+2;
	}
	// PPS
	if (pos+1>bufferLen)//Check size
	{
		Error("-H265: Failed to parse PPS count from descriptor! pos: %d, bufferLen: %d\n", pos, bufferLen);
		return false;
	}
	num = buffer[pos++];
	for (DWORD i=0;i<num;i++)
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

void HEVCDescriptor::AddSequenceParameterSet(const BYTE *data,DWORD size, BYTE nuh_layer_id)
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
	//add nul_layer_ids
	sps_nul_layer_ids.push_back(nuh_layer_id);
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

void HEVCDescriptor::AddParametersFromFrame(const BYTE *data,DWORD size, BYTE nuh_layer_id)
{
	//Chop into NALs
	while(size>4)
	{
		//Get nal size
		DWORD nalSize =  get4(data,0);
		//Get NAL start
		const BYTE* nal = data+4;
		//Depending on the type
		switch(nal[0] & 0xF)
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
				AddSequenceParameterSet(nal,nalSize, nuh_layer_id);
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
	//Clear nul_layer_id
	sps_nul_layer_ids.clear();
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

	//Get data
	buffer[pos++] = configurationVersion;
	buffer[pos++] = profileSpace;
	buffer[pos++] = tierFlag;
	buffer[pos++] = profileIndication;
	//DWORD profileCompatibilityIndication
	for (size_t i = 0; i < sizeof(profileCompatibilityIndication); ++i)
	{
		const BYTE bitOffset = i*8;
		buffer[pos++] = (profileCompatibilityIndication & (0xff << bitOffset)) >> bitOffset;
	}
	//WORD interopConstraints
	buffer[pos++] = interopConstraints & 0xff;
	buffer[pos++] = (interopConstraints & 0xff00) >> 8;
	buffer[pos++] = levelIndication;
	buffer[pos++] = NALUnitLength | 0xFC; // last 2 bits

	if (pos != MediaParameterSize)
	{
		Error("-H265: HEVCDescriptoer serialization error! pos: %d, MediaParameterSize: %d \n", pos, MediaParameterSize);
		return -1;
	}

	// VPS
	buffer[pos++] = numOfVideoParameterSets;
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
	buffer[pos++] = numOfSequenceParameterSets;
	//Debug("-H265: start to serialize SPS [pos: %d, SPS count: %d]\n", pos, numOfSequenceParameterSets);
	for (BYTE i=0;i<numOfSequenceParameterSets;i++)
	{
		//Set nul_layer_id 1B
		buffer[pos++] = sps_nul_layer_ids[i];
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
	buffer[pos++] = numOfPictureParameterSets;
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
	Debug("\tprofileSpace: %d\n",profileSpace);
	Debug("\ttierFlag: %d\n",tierFlag);
	Debug("\tprofileIndication: %d\n",profileIndication);
	Debug("\tprofileCompatibilityIndication: 0x%x\n",profileCompatibilityIndication);
	Debug("\tinteropConstraints: 0x%x\n",interopConstraints);
	Debug("\tlevelIndication: %d\n",levelIndication);
	Debug("\tNALUnitLength: %d\n",NALUnitLength);
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
		sps.Decode(spsData[i] + HEVCParams::RTP_NAL_HEADER_SIZE,spsSizes[i] - HEVCParams::RTP_NAL_HEADER_SIZE, sps_nul_layer_ids[i]);
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
