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
	virtual int GetWidth();
	virtual int GetHeight();

	virtual int Update(int index,BYTE *frame,int width,int heigth);
	virtual int Clean(int index);

private:
	BYTE*	underBuffer;
	BYTE*	under;
};
#endif
