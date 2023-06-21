/* 
 * File:   HEVCDescriptor.cpp
 * Author: Sergio
 * 
 * Created on 21 de junio de 2011, 12:21
 */
#include "log.h"
#include "HEVCdescriptor.h"
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
		Error("HEVCDescriptot is too short!");
		return false;
	}

	// read pointer
	DWORD pos = 0;

	//Get data
	configurationVersion 			= buffer[pos]; pos ++;
	profileSpace  					= buffer[pos]; pos ++;
	tierFlag   						= buffer[pos]; pos ++;
	profileIndication 				= buffer[pos]; pos ++;
	profileCompatibilityIndication	= buffer[pos]; pos ++;
	interopConstraints				= buffer[pos] + (buffer[pos + 1] << 8);
	pos += 2;
	levelIndication   				= buffer[pos]; pos ++;
	//NALUnitLength = buffer[4] & 0x03;
	NALUnitLength   				= buffer[pos] & 0x03;	
	pos ++;

	if (pos != MediaParameterSize)
	{
		Error(" HEVCDescriptoer parsing error!");
		return false;
	}

	//Clean all parameter sets
	ClearVideoParameterSets();
	ClearSequenceParameterSets();
	ClearPictureParameterSets();

	// VPS
	if( pos + 1 > bufferLen )//Check size
	{
		Error("-H265: Failed to parse VPS from descriptor");
		return false;
	}
	BYTE num = buffer[pos];
	pos++;
	for (BYTE i=0;i<num;i++)
	{
		if (pos+2 > bufferLen)//Check size
		{
			Error("-H265: Failed to parse VPS from descriptor");
			return false;
		}
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		if (pos+2+length>bufferLen)//Check size
		{
			Error("-H265: Failed to parse VPS from descriptor");
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
		Error("-H265: Failed to parse SPS from descriptor");
		return false;
	}
	num = buffer[pos];
	pos++;
	for (DWORD i=0;i<num;i++)
	{
		//Get nul_layer_id	
		if (pos+1>bufferLen)//Check size
		{
			Error("-H265: Failed to parse SPS from descriptor");
			return false;
		}
		BYTE nul_layer_id = buffer[pos];
		pos++;
		if (pos+2>bufferLen)//Check size
		{
			Error("-H265: Failed to parse SPS from descriptor");
			return false;
		}
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		
		if (pos+2+length>bufferLen)//Check size
		{
			Error("-H265: Failed to parse SPS from descriptor");
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
		Error("-H265: Failed to parse PPS from descriptor");
		return false;
	}
	num = buffer[pos];
	pos++;
	for (DWORD i=0;i<num;i++)
	{
		if (pos+2>bufferLen)//Check size
		{
			Error("-H265: Failed to parse PPS from descriptor");
			return false;
		}
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		
		if (pos+2+length>bufferLen)//Check size
		{
			Error("-H265: Failed to parse PPS from descriptor");
			return false;
		}
		//Add it
		AddPictureParameterSet(buffer+pos+2,length);
		//Update read pointer
		pos+=length+2;
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
	buffer[pos] = configurationVersion; pos++;
	buffer[pos] = profileSpace; pos++;
	buffer[pos] = tierFlag; pos++;
	buffer[pos] = profileIndication; pos++;
	buffer[pos] = profileCompatibilityIndication; pos++;
	buffer[pos] = interopConstraints & 0xff; pos++;
	buffer[pos] = (interopConstraints & 0xff00) >> 8; pos++;
	buffer[pos] = levelIndication; pos++;
	buffer[pos] = NALUnitLength | 0xFC; pos++;

	if (pos != MediaParameterSize)
	{
		Error(" HEVCDescriptoer serialization error!");
		return -1;
	}

	// VPS
	buffer[pos] = numOfVideoParameterSets;
	pos++;
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
	buffer[pos] = numOfSequenceParameterSets;
	pos++;
	for (BYTE i=0;i<numOfSequenceParameterSets;i++)
	{
		//Set nul_layer_id 1B
		buffer[pos] = sps_nul_layer_ids[i];
		pos++;
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
	buffer[pos] = numOfPictureParameterSets;
	pos++;
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
		vps.Decode(vpsData[i],vpsSizes[i]);
		//Dump
		vps.Dump();
	}
	//Dump SPS
	for (BYTE i=0;i<numOfSequenceParameterSets;i++)
	{
		H265SeqParameterSet sps;
		//Decode
		sps.Decode(spsData[i],spsSizes[i], sps_nul_layer_ids[i]);
		//Dump
		sps.Dump();
	}
	//Dump VPS
	for (BYTE i=0;i<numOfPictureParameterSets;i++)
	{
		H265PictureParameterSet pps;
		//Decode
		pps.Decode(spsData[i],spsSizes[i]);
		//Dump
		pps.Dump();
	}
	Debug("[HEVCDescriptor/]\n");
}
