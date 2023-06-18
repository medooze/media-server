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
	configurationVersion = 0;
	HEVCProfileIndication = 0;
	profileCompatibility = 0;
	HEVCLevelIndication = 0;
    numOfSequenceParameterSets = 0;
	numOfPictureParameterSets = 0;
	ppsTotalSizes = 0;
	spsTotalSizes = 0;
}

bool HEVCDescriptor::Parse(const BYTE* buffer,DWORD bufferLen)
{
	//Check size
	if (bufferLen<7)
		//Nothing
		return false;
	//Get data
	configurationVersion = buffer[0];
	HEVCProfileIndication = buffer[1];
	profileCompatibility = buffer[2];
	HEVCLevelIndication = buffer[3];
	NALUnitLength = buffer[4] & 0x03;
	//Get number of SPS
	DWORD num = buffer[5] & 0x1F;
	//Set size
	DWORD pos = 6;
	//Clean PPS
	ClearPictureParameterSets();
	//Clean SPS
	ClearSequenceParameterSets();
	//Read them
	for (DWORD i=0;i<num;i++)
	{
		//Check size
		if (pos+2>bufferLen)
			//exit
			return false;
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		//Check size
		if (pos+2+length>bufferLen)
			//exit
			return false;
		//Add it
		AddSequenceParameterSet(buffer+pos+2,length);
		//Skip them
		pos+=length+2;
	}
	//Check size
	if (pos+1>bufferLen)
		//exit
		return false;
	//Get number of PPS
	num = buffer[pos];
	//Skip
	pos++;
	//Read them
	for (DWORD i=0;i<num;i++)
	{
		//Check size
		if (pos+2>bufferLen)
			//exit
			return false;
		//Get length
		WORD length = ((WORD)(buffer[pos]))<<8 | buffer[pos+1];
		//Check size
		if (pos+2+length>bufferLen)
			//exit
			return false;
		//Add it
		AddPictureParameterSet(buffer+pos+2,length);
		//Skip them
		pos+=length+2;
	}

	return true;
}

HEVCDescriptor::~HEVCDescriptor()
{
	//REmove PPS
	ClearSequenceParameterSets();
	//Remove SPS
	ClearPictureParameterSets();
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
		switch(nal[0] & 0xF)
		{
			case 8:
				//Append
				AddPictureParameterSet(nal,nalSize);
				break;
			case 7:
				//Append
				AddSequenceParameterSet(nal,nalSize);
				break;
		}
		//Skip it
		data+=4+nalSize;
		size-=4+nalSize;
	}
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
	//Check size
	if (bufferLength<GetSize())
		//Nothing
		return -1;
	//Get data
	buffer[0] = configurationVersion ;
	buffer[1] = HEVCProfileIndication;
	buffer[2] = profileCompatibility;
	buffer[3] = HEVCLevelIndication;
	buffer[4] = NALUnitLength | 0xFC;
	buffer[5] = numOfSequenceParameterSets | 0xE0;
	//Set size
	DWORD pos = 6;
	//Read them
	for (BYTE i=0;i<numOfSequenceParameterSets;i++)
	{
		//Get length
		WORD length = spsSizes[i];
		//Set len
		buffer[pos]	= length >> 8;
		buffer[pos+1]	= length ;
		//Copy sps
		memcpy(buffer+pos+2,spsData[i],length);
		//Skip them
		pos+=length+2;
	}
	//Get number of PPS
	buffer[pos] = numOfPictureParameterSets;
	//Skip
	pos++;
	//Read them
	for (BYTE i=0;i<numOfPictureParameterSets;i++)
	{
		//Get length
		WORD length = ppsSizes[i];
		//Set len
		buffer[pos]	= length >> 8;
		buffer[pos+1]	= length;
		//Copy sps
		memcpy(buffer+pos+2,ppsData[i],length);
		//Skip them
		pos+=length+2;
	}
	//Finished
	return pos;
}

void HEVCDescriptor::Dump() const
{
	//Get data
	Debug("[HEVCDescriptor\n");
	Debug(" configurationVersion: %d\n",configurationVersion);
	Debug(" HEVCProfileIndication: 0x%.2x\n",HEVCProfileIndication);
	Debug(" profileCompatibility: 0x%.2x\n",profileCompatibility);
	Debug(" HEVCLevelIndication: 0x%.2x\n",HEVCLevelIndication);
	Debug(" NALUnitLength: %d\n",NALUnitLength);
	Debug(" numOfSequenceParameterSets: %d\n",numOfSequenceParameterSets);
	Debug(" numOfPictureParameterSets: %d\n",numOfPictureParameterSets);
	Debug("]");
	//Dump them
	for (BYTE i=0;i<numOfSequenceParameterSets;i++)
	{
		H265SeqParameterSet sps;
		//Decode
		// @Zita TODO: need extra nuh_layer_id from Nal header.
		sps.Decode(spsData[i],spsSizes[i], /*nuh_layer_id*/ 0);
		//Dump
		sps.Dump();
	}
	
	
	//Dump them
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
