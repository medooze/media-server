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
#include <set>
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
			return 7;
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
		case mosaic4x4:
			return 16;
		case mosaic1p4A:
			return 5;
		case mosaic1p2A:
			return 3;
		case mosaic1p2x2A:
			return 5;
		case mosaic1p6A:
			return 7;
		case mosaic1p12:
			return 13;
		case mosaic1p16A:
			return 17;
		case mosaic4x5A:
			return 20;
		case mosaic5x5:
			return 25;
		case mosaic1p1A:
			return 2;
		case mosaic1p2:
			return 3;
		case mosaic1p2x6A:
			return 13;
		case mosaic1p1p2x4A:
			return 10;
		case mosaic1p3A:
			return 4;
		case mosaic3x2:
			return 6;
	}
	//Error
	return Error("-Unknown mosaic type %d\n",type);
}

Mosaic::Mosaic(Type type,DWORD size) : overlay(::GetWidth(size),::GetHeight(size))
{
	//Get width and height
	mosaicTotalWidth = ::GetWidth(size);
	mosaicTotalHeight = ::GetHeight(size);

	//Store mosaic type
	mosaicType = type;

	//Calculate total size
	mosaicSize = mosaicTotalWidth*mosaicTotalHeight*3/2+AV_INPUT_BUFFER_PADDING_SIZE+32;
	//Allocate memory
	mosaicBuffer = (BYTE *) malloc32(mosaicSize);
	//Get aligned
	mosaic = ALIGNTO32(mosaicBuffer);

	//Clean mosaic
	DWORD mosaicNumPixels = mosaicTotalWidth*mosaicTotalHeight;
	BYTE *lineaY = mosaic;
	BYTE *lineaU = mosaic + mosaicNumPixels;
	BYTE *lineaV = lineaU + mosaicNumPixels/4;

	// paint the background in black
	memset(lineaY, 0, mosaicNumPixels);
	memset(lineaU, (BYTE) -128, mosaicNumPixels/4);
	memset(lineaV, (BYTE) -128, mosaicNumPixels/4);

	//Store number of slots
	numSlots = GetNumSlotsForType(type);

	//Allocate sizes
	mosaicSlots = (int*)malloc(numSlots*sizeof(int));
	mosaicPos   = (int*)malloc(numSlots*sizeof(int));
	oldPos	    = (int*)malloc(numSlots*sizeof(int));

	//Empty them
	memset(mosaicSlots,0,numSlots*sizeof(int));
	memset(mosaicPos,0,numSlots*sizeof(int));
	//Old pos are different so they are filled with logo on first pass
	memset(oldPos,-1,numSlots*sizeof(int));

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
	overlayUsed = false;

	//No vad particpant
	vadParticipant = 0;
	//Not blocked
	vadBlockingTime = 0;
	//Don't' hide
	hideVadParticipant = false;
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
	//Check old positions
	if (oldPos)
		//Free it
		free(oldPos);

	//Delete lingering participants
	for(Participants::iterator it = participants.begin(); it!=participants.end(); it++)
		//Delete participant info
		delete (it->second);
}

/************************
* SetSlot
*	Set slot participant
*************************/
int Mosaic::SetSlot(int num,int id)
{
	//Check num
	if (num>numSlots-1 || num<0)
		//Exit
		return Error("Slot not in mosaic [num:%d,numSlots:%d]\n",num,numSlots);

	//Log
	Log("-SetSlot [slot=%d,id=%d]\n",num,id);

	//Get old participant
	int oldId = mosaicSlots[num];

	//Set slot to participant id
	mosaicSlots[num] = id;

	//If we fix a differrent participant
	if (id>0 && oldId!=id)
	{
		//Find participant
		Participants::iterator it = participants.find(id);
		//If it is found
		if (it!=participants.end())
			//It is fixed
			it->second->isFixed++;
	} 

	//If it is not same participant
	if (oldId>0 && oldId!=id)
	{
		//Find participant
		Participants::iterator it = participants.find(oldId);
		//If it is found
		if (it!=participants.end())
			//It is fixed
			it->second->isFixed--;
	}
	
	//If it was an the VAD and now VAD is not shown
	if (oldId==SlotVAD && !IsVADShown())
	{
		//No vad particpant
		vadParticipant = 0;
		//Not blocked
		vadBlockingTime = 0;
	}

	//Evirything ok
	return 1;
}

int Mosaic::GetParticipantSlot(int id)
{
	for (int i=0; i<numSlots; ++i)
		if (mosaicSlots[i]==id)
			return i;
	return PositionNotShown;
}

int* Mosaic::GetPositions()
{
	//Return them
	return mosaicPos;
}

int* Mosaic::GetOldPositions()
{
	//Return them
	return oldPos;
}

QWORD Mosaic::GetVADBlockingTime()
{
	//Check if the position is fixed
	return vadBlockingTime;
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

QWORD  Mosaic::GetScore(int id)
{
	//Find it
	Participants::iterator it = participants.find(id);

	//If not found
	if (it==participants.end())
		//Exit
		return 0;

	//We have it
	return it->second->score;
}

int  Mosaic::SetScore(int id, QWORD score)
{
	//Find it
	Participants::iterator it = participants.find(id);

	//If not found
	if (it==participants.end())
		//Exit
		return 0;
	
	//Get info
	PartInfo *info = it->second;

	//Remove from order
	order.erase(info);

	//Set new score
	info->score = score;

	//Add it again so it is ordered
	order.insert(info);

	//We have it
	return 1;
}

int Mosaic::AddParticipant(int id,QWORD score)
{
	//Log
	Log("-AddParticipant [id:%d,score:%llu]\n",id,score);

	//Chck if allready added
	Participants::iterator it = participants.find(id);

	//If it wasa already presetn
	if (it!=participants.end())
		//Error
		return Error("Participant already in Mosaic\n");

	//Create new participant info
	PartInfo *info = new PartInfo(id,score,0);
		
	//Set it
	participants[id] = info;

	//Add to order set
	order.insert(info);
	
	//Return 
	return 1;
}

int Mosaic::RemoveParticipant(int id)
{
	Log("-RemoveParticipant [id:%d]\n",id);

	//Find it
	Participants::iterator it = participants.find(id);

	//If not found
	if (it==participants.end())
		//Exit
		return Error("-Participant not found\n");

	//Get info
	PartInfo *info = it->second;

	//Remove it for the list
	participants.erase(it);

	//Remove from order set
	order.erase(info);

	//Delete it
	delete(info);

	//If it was the vad
	if (id==vadParticipant)
	{
		//Reset vad
		vadParticipant = 0;
		vadBlockingTime = 0;
	}
	//Check all slots
	for (int i=0;i<numSlots;i++)
	{
		//Check if it was ours
		if (mosaicSlots[i]==id)
		{
			//Set mosaic slot free
			mosaicSlots[i] = SlotFree;
			//Set empty pos
			mosaicPos[i] = 0;
		}
	}

	//Return position
	return 1;
}

bool Mosaic::IsVADShown()
{
	//For each slot
	for (int i=0; i<numSlots; ++i)
		//Check if it is VAD
		if (mosaicSlots[i]==SlotVAD)
			//Shown
			return true;
	//Not shown
	return false;
}

int Mosaic::GetVADParticipant()
{
	return vadParticipant;
}


void Mosaic::SetVADParticipant(int id,bool hide,QWORD blockedUntil)
{
	//Set it
	vadParticipant = id;
	//Set block time
	vadBlockingTime = blockedUntil;
	//Store if it is hidden
	hideVadParticipant = hide;
}

int Mosaic::CalculatePositions()
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Clean all positions
	memset(mosaicPos,0,numSlots*sizeof(int));

	//Get ordererd participants
	ParticipantsOrder::iterator it = order.begin();

	//For each slot
	for (int i=0; i<numSlots; ++i)
	{
		//Depending the slot availability
		switch (mosaicSlots[i])
		{
			case SlotFree:
				//Check if we have more participants to draw
				while (it!=order.end())
				{
					//Get info
					PartInfo* info = *it;
					//Move to next participant
					++it;
					//Check if it is the VAD participnat and we have to hide it or if it is fixed
					if ((info->id==vadParticipant && hideVadParticipant && IsVADShown()) || info->isFixed>0)
					{
						Log("-skiping part %d %d %d %d\n",info->id,vadParticipant,hideVadParticipant,info->isFixed);
						//Next one
						continue;
					}
					//Put it in the position
					mosaicPos[i] = info->id;
					//found
					break;
				}
				break;
			case SlotLocked:
				//Nothing to do
				break;
			case SlotVAD:
				//Check if we have VAD participant
				if (vadParticipant)
					//Put it in the position
					mosaicPos[i] = vadParticipant;
				break;
			default:
				//Double check that the participant is in the mosaic yet
				if (participants.find(mosaicSlots[i])!=participants.end())
				{
					//Fix the participant
					mosaicPos[i] = mosaicSlots[i];
				} else {
					//Set slot free
					mosaicSlots[i] = SlotFree;
					//Check if we have more participants to draw
					while (it!=order.end())
					{
						//Get info
						PartInfo* info = *it;
						//Move to next participant
						++it;
						//Check if it is the VAD participnat and we have to hide it or if it is fixed
						if ((info->id==vadParticipant && hideVadParticipant && IsVADShown()) || info->isFixed>0)
							//Next one
							continue;
						//Put it in the position
						mosaicPos[i] = info->id;
						//found
						break;
					}
				}
				break;
		}
	}

	return 1;
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

	//For each slot
	for (int i=0;i<numSlots && i<num;i++)
	{
		//Get id
		int id = slots[i];
		//If we fix a participant
		if (id>0)
		{
			//Find participant
			Participants::iterator it = participants.find(id);
			//If it is found
			if (it!=participants.end())
				//It is fixed
				it->second->isFixed++;
		}
		//Set slot position
		mosaicSlots[i] = id;
	}
}


Mosaic* Mosaic::CreateMosaic(Type type,DWORD size)
{
	//Create mosaic depending on composition
	switch(type)
	{
		case mosaic1x1:
		case mosaic2x2:
		case mosaic3x3:
		case mosaic4x4:
		case mosaic5x5:
		case mosaic4x5A:
		case mosaic1p1A:
			//Set mosaic
			return new PartedMosaic(type,size);
		case mosaic1p1:
		case mosaic3p4:
		case mosaic1p7:
		case mosaic1p5:
		case mosaic1p4A:
		case mosaic1p2A:
		case mosaic1p2x2A:
		case mosaic1p6A:
		case mosaic1p12:
		case mosaic1p16A:
		case mosaic1p2:
		case mosaic1p2x6A:
		case mosaic1p1p2x4A:
		case mosaic1p3A:
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
	//Lock method
	ScopedLock scoped(mutex); 
	
	//Check if there is a overlay
	if (!overlayUsed)
		//Return mosaic without change
		return mosaic;
	//Check if we need to change
	if (overlayNeedsUpdate)
		//Calculate and return
		return overlay.Display(mosaic);
	//Return overlay
	return overlay.GetOverlay();
}

void Mosaic::Reset()
{
	//Not changed anymore
	mosaicChanged = false;

	//Move old position to new one
	memcpy(oldPos,mosaicPos,numSlots*sizeof(int));
}

int Mosaic::SetOverlayPNG(const char* filename)
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Log
	Log("-SetOverlay [%s,%d,%d]\n",filename,mosaicTotalWidth,mosaicTotalHeight);

	//And load it
	if(!overlay.LoadPNG(filename))
		//Error
		return Error("Error loading png image\n");
	//Using overlay
	overlayUsed = true;
	//Display it
	overlayNeedsUpdate = true;

	//OK
	return 1;
}

int Mosaic::SetOverlaySVG(const char* svg)
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Log
	Log("-SetOverlay [%s]\n",svg);

	//And load it
	if(!overlay.LoadSVG(svg))
		//Error
		return Error("Error loading png image");
	//Using overlay
	overlayUsed = true;
	//Display it
	overlayNeedsUpdate = true;
	//OK
	return 1;
}

int Mosaic::SetOverlayText()
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Clean it
	overlay.Reset();
	//Using overlay
	overlayUsed = true;
	//Display it
	overlayNeedsUpdate = true;
	//OK
	return 1;
}

int Mosaic::RenderOverlayText(const std::wstring& text,DWORD x,DWORD y,DWORD width,DWORD height, const Properties& properties)
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Render text
	return overlay.RenderText(text,x,y,width,height,properties);
}

int Mosaic::RenderOverlayText(const std::string& utf8,DWORD x,DWORD y,DWORD width,DWORD height, const Properties& properties)
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Render text
	return overlay.RenderText(utf8,x,y,width,height,properties);
}

int Mosaic::ResetOverlay()
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Log
	Log("-Reset overaly\n");
	//Clean it
	overlay.Reset();
	//Not using overlay
	overlayUsed = false;
	//Display it
	overlayNeedsUpdate = false;
	//OK
	return 1;
}

int Mosaic::DrawVUMeter(int pos,DWORD val,DWORD size)
{
	//Lock method
	ScopedLock scoped(mutex);
	
	//Get dimensions for slot
	DWORD width = GetWidth(pos);
	DWORD height = GetHeight(pos);
	//Get mosaic dimension
	DWORD totalWidth = GetWidth();
	DWORD totalHeight = GetHeight();
	//Get initial pos
	DWORD top = GetTop(pos);
	DWORD left = GetLeft(pos);
	//Calculate total pixels
	DWORD numPixels = totalWidth*totalHeight;
	//Get data planes
	BYTE *y = mosaic;
	BYTE *u = y+numPixels;
	BYTE *v = u+numPixels/4;

	//Get init of xVU meter
	int i = (left+9) & 0xFFFFFFF8;
	int j = (top+height-10) & 0xFFFFFFFE;
	//Set dimensions
	int w = (width-16) & 0xFFFFFFF0;
	int m = ((w-4)*val)/size & 0xFFFFFFFC;

	//Write top border
	for (int k=0;k<1;k++,j+=2)
	{
		memset(y+j*totalWidth+i			,0,w);
		memset(y+(j+1)*totalWidth+i		,0,w);
		memset(u+(j*totalWidth)/4+i/2		,-64,w/2);
		memset(v+(j*totalWidth)/4+i/2		,-64,w/2);
	}

	//Write VU
	for (int k=0;k<2;k++,j+=2)
	{
		//Left border
		memset(y+j*totalWidth+i			,0,2);
		memset(y+(j+1)*totalWidth+i		,0,2);
		memset(u+(j*totalWidth)/4+i/2		,-64,1);
		memset(v+(j*totalWidth)/4+i/2		,-64,1);
		//VU
		memset(y+j*totalWidth+i+2		,160,m);
		memset(y+(j+1)*totalWidth+i+2		,160,m);
		memset(u+(j*totalWidth)/4+i/2+2		,160,m/2);
		memset(v+(j*totalWidth)/4+i/2+2		,160,m/2);
		//VU
		memset(y+j*totalWidth+i+m+2		,0,w-m-2);
		memset(y+(j+1)*totalWidth+i+m+2		,0,w-m-2);
		memset(u+(j*totalWidth)/4+(m+i+2)/2	,-64,(w-m)/2-1);
		memset(v+(j*totalWidth)/4+(m+i+2)/2	,-64,(w-m)/2-1);
	}

	//Write bottom border
	for (int k=0;k<1;k++,j+=2)
	{
		memset(y+j*totalWidth+i			,0,w);
		memset(y+(j+1)*totalWidth+i		,0,w);
		memset(u+(j*totalWidth)/4+i/2		,-64,w/2);
		memset(v+(j*totalWidth)/4+i/2		,-64,w/2);
	}

	return 1;
}

bool Mosaic::SetPadding(int top, int right, int bottom, int left)
{
	//Check positive values
	if (top<0 || right<0 || bottom<0 || left<0)
		return false;
	//Check enought widht
	if (left+right>mosaicTotalWidth)
		return false;
	//Check enough height
	if (top+bottom>mosaicTotalHeight)
		return false;
	//Store padding
	paddingTop = top;
	paddingRight = right;
	paddingBottom = bottom;
	paddingLeft = left;
	//Done
	return true;
}
