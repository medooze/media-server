#ifndef _MOSAIC_H_
#define _MOSAIC_H_
#include "config.h"
#include "framescaler.h"
#include "overlay.h"
#include "vad.h"
#include <map>

class Mosaic
{
public:
	static const int NotShown = -1;
	static const int NotFound = -2;

	static const int SlotFree     = 0;
	static const int SlotLocked   = -1;
	static const int SlotVAD      = -2;
	typedef enum
	{
		mosaic1x1	= 0,
		mosaic2x2	= 1,
		mosaic3x3	= 2,
		mosaic3p4	= 3,
		mosaic1p7	= 4,
		mosaic1p5	= 5,
		mosaic1p1	= 6,
		mosaicPIP1	= 7,
		mosaicPIP3	= 8,
		mosaic4x4	= 9,
	} Type;

	class PartInfo
	{
	    public:
		int vadLevel;
		bool kickable;
		bool eligible;
	};

public:
	Mosaic(Type type,DWORD size);
	virtual ~Mosaic();

	int GetWidth()		{ return mosaicTotalWidth;}
	int GetHeight()		{ return mosaicTotalHeight;}
	int HasChanged()	{ return mosaicChanged; }
	void Reset()		{ mosaicChanged = true; }

	BYTE* GetFrame();
	virtual int Update(int index,BYTE *frame,int width,int heigth) = 0;
	virtual int Clean(int index) = 0;

	int AddParticipant(int id);
	int HasParticipant(int id);
	int RemoveParticipant(int id);
	int SetSlot(int num,int id);
	int SetSlot(int num,int id,QWORD blockedUntil);
	QWORD GetBlockingTime(int num);

	int GetPosition(int id);
	int GetVADPosition();
	int* GetPositions();
	int* GetSlots();
	int GetNumSlots();
	void SetSlots(int *slots,int num);
	bool IsFixed(DWORD pos);

	int GetVADParticipant();
	int SetVADParticipant(int id,QWORD blockedUntil);

	static int GetNumSlotsForType(Type type);
	static Mosaic* CreateMosaic(Type type,DWORD size);

	int SetOverlayPNG(const char* filename);
	int SetOverlaySVG(const char* svg);
	int ResetOverlay();
	int DrawVUMeter(int pos,DWORD val,DWORD size);

	int UpdateParticipantInfo(int id, int vadLevel);
	int CalculatePositions();
protected:
	virtual int GetWidth(int pos) = 0;
	virtual int GetHeight(int pos) = 0;
	virtual int GetTop(int pos) = 0;
	virtual int GetLeft(int pos) = 0;
protected:
	void SetChanged()	{ mosaicChanged = true; overlayNeedsUpdate = true; }

protected:
	typedef std::map<int,int> Participants;
	typedef std::map<int,PartInfo> ParticipantInfos;

protected:
	Participants participants;
	ParticipantInfos partVad;
	int mosaicChanged;

	// information on whether slot is locked, free, fixed (= id of participant), vad
	int *mosaicSlots;

	// association between position and ids
	int *mosaicPos;
	QWORD *mosaicSlotsBlockingTime;
	QWORD *oldTimes;
	int numSlots;
	int vadParticipant;

	FrameScaler** resizer;
	BYTE* 	mosaic;
	BYTE*	mosaicBuffer;
	int 	mosaicTotalWidth;
	int 	mosaicTotalHeight;
	Type	mosaicType;
	int     mosaicSize;

	Overlay* overlay;
	bool	 overlayNeedsUpdate;

protected:
	int GetNextFreeSlot(int id);
};

#endif
