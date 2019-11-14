#ifndef _PIPMOSAIC_H_
#define _PIPMOSAIC_H_

#include "mosaic.h"
#include "framescaler.h"

class PIPMosaic:
	public Mosaic
{
public:
	PIPMosaic(Mosaic::Type type, DWORD size);
	virtual ~PIPMosaic();

	virtual int GetSlots();

	virtual BYTE* GetFrame();

	virtual int Update(int index,BYTE *frame,int width,int heigth, bool keepAspectRatio);
	virtual int Clean(int index);

	virtual int GetWidth(int pos);
	virtual int GetHeight(int pos);
	virtual int GetTop(int pos);
	virtual int GetLeft(int pos);
private:
	BYTE*	underBuffer;
	BYTE*	under;
};
#endif
