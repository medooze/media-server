/* 
 * File:   AVCDescriptor.cpp
 * Author: Sergio
 * 
 * Created on 21 de junio de 2011, 12:21
 */
#include "log.h"
#include "avcdescriptor.h"
#include "tools.h"
#include "h264/h264.h"
#include <string.h>
#include <stdlib.h>
#include <vector>

AVCDescriptor::AVCDescriptor()
{
	//Empty
	configurationVersion = 0;
	AVCProfileIndication = 0;
	profileCompatibility = 0;
	AVCLevelIndication = 0;
    	numOfSequenceParameterSets = 0;
	numOfPictureParameterSets = 0;
	ppsTotalSizes = 0;
	spsTotalSizes = 0;
}

bool AVCDescriptor::Parse(BYTE* buffer,DWORD bufferLen)
{
	//Check size
	if (bufferLen<7)
		//Nothing
		return false;
	//Get data
	configurationVersion = buffer[0];
	AVCProfileIndication = buffer[1];
	profileCompatibility = buffer[2];
	AVCLevelIndication = buffer[3];
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
	for (int i=0;i<num;i++)
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
	for (int i=0;i<num;i++)
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

AVCDescriptor::~AVCDescriptor()
{
	//REmove PPS
	ClearSequenceParameterSets();
	//Remove SPS
	ClearPictureParameterSets();
}

void AVCDescriptor::AddSequenceParameterSet(BYTE *data,DWORD size)
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

void AVCDescriptor::AddParametersFromFrame(BYTE *data,DWORD size)
{
	//Chop into NALs
	while(size>4)
	{
		//Get nal size
		DWORD nalSize =  get4(data,0);
		//Get NAL start
		BYTE *nal = data+4;
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

void AVCDescriptor::AddPictureParameterSet(BYTE *data,DWORD size)
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

void  AVCDescriptor::ClearSequenceParameterSets()
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

void  AVCDescriptor::ClearPictureParameterSets()
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

DWORD AVCDescriptor::Serialize(BYTE* buffer,DWORD bufferLength) const
{
	//Check size
	if (bufferLength<GetSize())
		//Nothing
		return -1;
	//Get data
	buffer[0] = configurationVersion ;
	buffer[1] = AVCProfileIndication;
	buffer[2] = profileCompatibility;
	buffer[3] = AVCLevelIndication;
	buffer[4] = NALUnitLength | 0xFC;
	buffer[5] = numOfSequenceParameterSets | 0xE0;
	//Set size
	DWORD pos = 6;
	//Read them
	for (int i=0;i<numOfSequenceParameterSets;i++)
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
	for (int i=0;i<numOfPictureParameterSets;i++)
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

void AVCDescriptor::Dump() const
{
	//Get data
	Debug("-Dumping AVC Descriptor\n");
	Debug(" configurationVersion: %d\n",configurationVersion);
	Debug(" AVCProfileIndication: 0x%.2x\n",AVCProfileIndication);
	Debug(" profileCompatibility: 0x%.2x\n",profileCompatibility);
	Debug(" AVCLevelIndication: 0x%.2x\n",AVCLevelIndication);
	Debug(" NALUnitLength: %d\n",NALUnitLength);
	Debug(" numOfSequenceParameterSets: %d\n",numOfSequenceParameterSets);
	//Dump them
	for (int i=0;i<numOfSequenceParameterSets;i++)
	{
		Debug(" SequenceParameterSets[%d]\n",i);
		try {
			H264SeqParameterSet sps;
			//Decode
			sps.Decode(spsData[i],spsSizes[i]);
			//Dump
			sps.Dump();
		} catch (...) {
			Debug(" exception raised while parsing sqs");
			//Dump
			::Dump(spsData[i],spsSizes[i]);
		}
	}
	
	Debug(" numOfPictureParameterSets: %d\n",numOfPictureParameterSets);
	//Dump them
	for (int i=0;i<numOfPictureParameterSets;i++)
	{
		Debug(" PictureParameterSets[%d]\n",i);
		try {
			H264PictureParameterSet pps;
			//Decode
			pps.Decode(spsData[i],spsSizes[i]);
			//Dump
			pps.Dump();
		} catch (...) {
			Debug(" exception raised while parsing pps");
			//Dump
			::Dump(spsData[i],spsSizes[i]);
		}
	}
}
