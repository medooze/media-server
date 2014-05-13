#ifndef _PARTEDMOSAIC_H_
#define _PARTEDMOSAIC_H_

#include "mosaic.h"
#include "framescaler.h"

class PartedMosaic:
	public Mosaic
{
public:
	PartedMosaic(Mosaic::Type type, DWORD size);
	virtual ~PartedMosaic();

	virtual int Update(int index,BYTE *frame,int width,int heigth, bool keepAspectRatio);
	virtual int Clean(int index);
	virtual int GetWidth(int pos);
	virtual int GetHeight(int pos);
	virtual int GetTop(int pos);
	virtual int GetLeft(int pos);
private:
	int     mosaicCols;
	int     mosaicRows;
	int     mosaicNum;
	int     mosaicWidth;
	int     mosaicHeight;
};

#endif
