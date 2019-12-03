#ifndef _MOSAIC_H_
#define _MOSAIC_H_
#include "config.h"
#include "framescaler.h"
#include "overlay.h"
#include "vad.h"
#include "logo.h"
#include "use.h"
#include <map>
#include <set>

class Mosaic
{
public:
	static const int PositionNotShown = -1;
	static const int PositionNotFound = -2;

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
		mosaic1p4A	= 10,
		mosaic1p2A	= 11,
		mosaic1p2x2A	= 12,
		mosaic1p6A	= 13,
		mosaic1p12	= 14,
		mosaic1p16A	= 15,
		mosaic4x5A	= 16,
		mosaic5x5	= 17,
		mosaic1p1A	= 18,
		mosaic1p2	= 19,
		mosaic1p2x6A    = 20,
		mosaic1p1p2x4A  = 21,
		mosaic1p3A      = 22,
		mosaic3x2       = 23
	} Type;

	struct PartInfo
	{
		PartInfo(int id,QWORD score,int isFixed)
		{
			this->id	= id;
			this->score	= score;
			this->isFixed	= isFixed;
		}
		
		int	id;
		QWORD	score;		//Score SHALL be unique
		int	isFixed;

		int	GetId() const 		{ return id;	}
		QWORD	GetScore() const	{ return score;		}
		int	GetIsFixed() const	{ return isFixed;	} 

		struct Short
		{
			//Bigger score first
			bool operator()(const PartInfo* a, const PartInfo* b) const
			{
				return a->score>b->score;
			}
		};
	};
public:
	typedef std::map<int,PartInfo*> Participants;
	typedef std::set<PartInfo*,PartInfo::Short> ParticipantsOrder;

public:
	static int GetNumSlotsForType(Type type);
	static Mosaic* CreateMosaic(Type type,DWORD size);

public:
	Mosaic(Type type,DWORD size);
	virtual ~Mosaic();

	int GetWidth()		const { return mosaicTotalWidth;}
	int GetHeight()		const { return mosaicTotalHeight;}
	int GetInnerWidth()	const { return mosaicTotalWidth - paddingLeft - paddingRight;	}
	int GetInnerHeight()	const { return mosaicTotalHeight - paddingTop - paddingBottom;	}
	
	int GetPaddingLeft()	const { return paddingLeft;	}
	int GetPaddingRight()	const { return paddingRight;	}
	int GetPaddingTop()	const { return paddingTop;	}
	int GetPaddingBottom()	const { return paddingBottom;	}
	
	int HasChanged()	const { return mosaicChanged;	}

	BYTE* GetFrame();
	virtual int Update(int index,BYTE *frame,int width,int heigth, bool keepAspectRatio = true) = 0;
	virtual int Clean(int index) = 0;
	virtual int Clean(int index,const Logo& logo)
	{
		//Check logo
		if (logo.GetFrame())
			//Print logo
			return Update(index,logo.GetFrame(),logo.GetWidth(),logo.GetHeight(),false);
		else
			//Clean
			return Clean(index);
	}

	int AddParticipant(int id,QWORD score);
	int HasParticipant(int id);
	int GetParticipantSlot(int id);
	int RemoveParticipant(int id);
	int SetSlot(int num,int id);
	QWORD GetScore(int id);
	int SetScore(int id, QWORD score);

	int CalculatePositions();
	void Reset();

	int* GetPositions();
	int* GetOldPositions();
	int* GetSlots();
	int GetNumSlots();
	void SetSlots(int *slots,int num);

	int GetVADParticipant();
	void SetVADParticipant(int id,bool hide,QWORD blockedUntil);
	QWORD GetVADBlockingTime();
	bool IsVADShown();

	int SetOverlayPNG(const char* filename);
	int SetOverlaySVG(const char* svg);
	int SetOverlayText();
	int RenderOverlayText(const std::wstring& text,DWORD x,DWORD y,DWORD width,DWORD height, const Properties &properties);
	int RenderOverlayText(const std::string& text,DWORD x,DWORD y,DWORD width,DWORD height, const Properties &properties);
	int ResetOverlay();
	int DrawVUMeter(int pos,DWORD val,DWORD size);
	
	bool SetPadding(int top, int right, int bottom, int left);
	
	virtual int GetWidth(int pos) = 0;
	virtual int GetHeight(int pos) = 0;
	virtual int GetTop(int pos) = 0;
	virtual int GetLeft(int pos) = 0;
	const Participants& GetParticipants() { return participants; }
	const ParticipantsOrder& GetParticipantsOrder()	{ return order; }
	Type  GetType() { return mosaicType;	}
protected:
	void SetChanged()	{ mosaicChanged = true; overlayNeedsUpdate = true; }


protected:
	Mutex			mutex;
	Participants		participants;
	ParticipantsOrder	order;
	int mosaicChanged;
	int numSlots;

	// information on whether slot is locked, free, fixed (= id of participant), vad
	int *mosaicSlots;

	// association between position and ids
	int *mosaicPos;
	int *oldPos;
	QWORD vadBlockingTime;
	
	int vadParticipant;
	bool hideVadParticipant;

	FrameScaler** resizer;
	BYTE* 	mosaic;
	BYTE*	mosaicBuffer;
	int 	mosaicTotalWidth;
	int 	mosaicTotalHeight;
	Type	mosaicType;
	int     mosaicSize;

	Overlay  overlay;
	bool	 overlayUsed;
	bool	 overlayNeedsUpdate;
	
	int	paddingLeft	= 0;
	int	paddingRight	= 0;
	int	paddingTop	= 0;
	int	paddingBottom	= 0;
};

#endif
