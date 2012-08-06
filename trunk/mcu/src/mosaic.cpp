#include "log.h"
#include "mosaic.h"
#include "partedmosaic.h"
#include "asymmetricmosaic.h"
#include "pipmosaic.h"
#include <stdexcept>
#include <vector>
#include <map>
#include <string.h>
#include <stdlib.h>
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
}

int Mosaic::GetNumSlotsForType(Mosaic::Type type)
{
	//Depending on the type
	switch(type)
	{
		case mosaic1x1:
			return 1;
		case mosaic2x2:
			return 4;
		case mosaic3x3:
			return 9;
		case mosaic3p4:
			return  7;
		case mosaic1p7:
			return 8;
		case mosaic1p5:
			return  6;
		case mosaic1p1:
			return 2;
		case mosaicPIP1:
			return 2;
		case mosaicPIP3:
			return 4;
	}
	//Error
	return 0;
}

Mosaic::Mosaic(Type type,DWORD size)
{
	//Get width and height
	mosaicTotalWidth = ::GetWidth(size);
	mosaicTotalHeight = ::GetHeight(size);

	//Store mosaic type
	mosaicType = type;

	//Calculate total size
	mosaicSize = mosaicTotalWidth*mosaicTotalHeight*3/2+FF_INPUT_BUFFER_PADDING_SIZE+32;
	//Allocate memory
	mosaicBuffer = (BYTE *) malloc(mosaicSize);
	//Get aligned
	mosaic = ALIGNTO32(mosaicBuffer);
	//Reset mosaic
	memset(mosaicBuffer ,(BYTE)-128,mosaicSize);

	//Store number of slots
	numSlots = GetNumSlotsForType(type);

	//Allocate sizes
	mosaicSlots = (int*)malloc(numSlots*sizeof(int));
	mosaicPos   = (int*)malloc(numSlots*sizeof(int));

	//Empty them
	memset(mosaicSlots,0,numSlots*sizeof(int));
	memset(mosaicPos,0,numSlots*sizeof(int));

	//Alloc resizers
	resizer = (FrameScaler**)malloc(numSlots*sizeof(FrameScaler*));
	
	//Create each resize
	for (int pos=0;pos<numSlots;pos++)
		//New scaler
		resizer[pos] = new FrameScaler();

	//Not changed
	mosaicChanged = false;
	
	//NOt need to add overlay
	overlayNeedsUpdate = false;

	//No overlay
	overlay = false;
}

Mosaic::~Mosaic()
{
	//Free memory
	free(mosaicBuffer);

	//Delete each resize
	for (int i=0;i<numSlots;i++)
		//Delete
		delete resizer[i];

	//Free memory
	free(resizer);

	//If already have slot list
	if (mosaicSlots)
		//Delete it
		free(mosaicSlots);

	//If already have position list
	if (mosaicPos)
		//Delete it
		free(mosaicPos);
}

/************************
* SetSlot
*	Set slot participant
*************************/
int Mosaic::SetSlot(int num,int id)
{
	Log(">SetPosition [num:%d,id:%d,max:%d]\n",num,id,numSlots);

	//Check num
	if (num>numSlots-1 || num<0)
		//Exit
		return Error("Slot not in mosaic \n");

	//If we fix a participant
	if (id>0)
	{
		//Find participant
		Participants::iterator it = participants.find(id);

		//If it is found
		if (it!=participants.end())
		{
			//Get position
			int pos = it->second;
			//Ensure it is inside bounds and was shown
			if (pos>=0 && pos<numSlots)
				//Set the old position free
				mosaicSlots[pos] = 0;
		}
	}

	//Set it
	mosaicSlots[num] = id;

	//Calculate positions
	CalculatePositions();

	Log("<SetPosition\n");

	return 1;
}

/************************
* GetPositions
*	Get position for participant
*************************/
int* Mosaic::GetPositions()
{
	//Return them
	return mosaicPos;
}

/************************
* GetPosition
*	Get position for participant
*************************/
int Mosaic::GetPosition(int id)
{
	//Find it
	Participants::iterator it = participants.find(id);

	//If not found
	if (it==participants.end())
		//Exit
		return -1;

	//Get position
	return it->second;
}

int Mosaic::HasParticipant(int id)
{
	//Find it
	Participants::iterator it = participants.find(id);

	//If not found
	if (it==participants.end())
		//Exit
		return 0;

	//We have it
	return 1;
}

int Mosaic::AddParticipant(int id)
{
	//Chck if allready added
	Participants::iterator it = participants.find(id);

	//If it was
	if (it!=participants.end())
		//Return it
		return it->second;

	//Not shown by defautl
	int pos = -1;

	//Look in the slots
	for (int i=0;i<numSlots;i++)
	{
		//It's lock for me or it is free
		if ((mosaicSlots[i]==id) || (mosaicSlots[i]==0 && mosaicPos[i]==0))
		{
			//Set our position
			mosaicPos[i]=id;
			//Set it also in the participant
			pos = i;
			//Next
			break;
		}
	}

	//Set position
	participants[id] = pos;

	Log("-AddParticipant [id:%d,pos:%d]\n",id,pos);
	
	//Return it
	return pos;
}

int Mosaic::RemoveParticipant(int id)
{
	//Find it
	Participants::iterator it = participants.find(id);
	
	//If not found 
	if (it==participants.end())
		//Exit
		return -1;
	
	//Get position
	int pos = it->second;
	
	//Remove it for the list
	participants.erase(it);
	
	//If  was shown and fixed
	if (pos>=0 && mosaicSlots[pos]==id)
		//lock it
		mosaicSlots[pos]=-1;

	//Recalculate positions
	CalculatePositions();
	
	//Return position
	return pos;
}

int Mosaic::CalculatePositions()
{
	//Get number of available slots
	int numSlots = GetNumSlots();

	//First erase positions
	memset(mosaicPos,0,numSlots*sizeof(int));

	//Start from the begining
	int first = 0;

	//Iterate Videos
	for (Participants::iterator it=participants.begin(); it!=participants.end(); ++it)
	{
		//Get id
		int id = it->first;
		//Clean position
		it->second = -1;

		//If we have been over the end
		if (first>numSlots)
		{
			//Not shown
			it->second = -1;
			//Continue with next
			continue;
		}
		
		//Not found the first free one
		int firstFree = -1;

		//Look in the slots
		for (int i=first;i<numSlots;i++)
		{
			//It's lock for me?
			if (mosaicSlots[i]==id)
			{
				//Set our position
				mosaicPos[i]=id;
				//Set it also in the participant
				it->second = i;
				//If we were the first one
				if (first==i)
					//Start after me next pass
					first++;
				//It's locked
				firstFree = -1;
				//Next
				break;
			} else if (mosaicSlots[i]==-1 && i==first) {
				//If the first one was a locked one skip it next time
				first++;
			} else if (mosaicSlots[i]==0 && mosaicPos[i]==0 && firstFree==-1) {
				//If it's free and we have still not a free one save it
				firstFree=i;
			} else if (mosaicPos[i]>0 && i==first) {
				//The first one was already calculated
				first++;
			}
		}

		//I have not fixed position check if there is any free slot
		if (firstFree!=-1)
		{
			//Here we are
			mosaicPos[firstFree]=id;
			//Set it also in the participant
			it->second = firstFree;
			//If we were the first one
			if (first==firstFree)
				//Start after me next pass
				first++;
		} else {

		}
	}
}

int* Mosaic::GetSlots()
{
	return mosaicSlots;
}

int Mosaic::GetNumSlots()
{
	return numSlots;
}

void Mosaic::SetSlots(int *slots,int num)
{
	//Reset slot list
	memset(mosaicSlots,0,numSlots*sizeof(int));

	//If we have old slots
	if (!slots)
		return;

	//Which was bigger?
	if (num<numSlots)
		//Copy
		memcpy(mosaicSlots,slots,num*sizeof(int));
	else
		//Copy
		memcpy(mosaicSlots,slots,numSlots*sizeof(int));
}


Mosaic* Mosaic::CreateMosaic(Type type,DWORD size)
{
	//Create mosaic depending on composition
	switch(type)
	{
		case Mosaic::mosaic1x1:
		case Mosaic::mosaic2x2:
		case Mosaic::mosaic3x3:
			//Set mosaic
			return new PartedMosaic(type,size);
		case Mosaic::mosaic1p1:
		case Mosaic::mosaic3p4:
		case Mosaic::mosaic1p7:
		case Mosaic::mosaic1p5:
			//Set mosaic
			return new AsymmetricMosaic(type,size);
		case mosaicPIP1:
		case mosaicPIP3:
			return new PIPMosaic(type,size);
	}
	//Exit
	throw new std::runtime_error("Unknown mosaic type\n");
}

BYTE* Mosaic::GetFrame()
{
	//Check if there is a overlay
	if (!overlay)
		//Return mosaic without change
		return mosaic;
	//Check if we need to change
	if (overlayNeedsUpdate)
		//Calculate and return
		return overlay->Display(mosaic);
	//Return overlay
	return overlay->GetOverlay();
}

int Mosaic::SetOverlayPNG(const char* filename)
{
	//Log
	Log("-SetOverlay [%s]\n",filename);

	//Reset any previous one
	if (overlay)
		//Delete it
		delete(overlay);
	//Create new one
	overlay = new Overlay(mosaicTotalWidth,mosaicTotalHeight);
	//And load it
	if(!overlay->LoadPNG(filename))
		//Error
		return Error("Error loading png image");
	//Display it
	overlayNeedsUpdate = true;
}
int Mosaic::SetOverlaySVG(const char* svg)
{
	//Nothing yet
	return false;
}
int Mosaic::ResetOverlay()
{
	//Log
	Log("-Reset overaly\n");
	//Reset any previous one
	if (overlay)
		//Delete it
		delete(overlay);
	//remove it
	overlay = false;
}
