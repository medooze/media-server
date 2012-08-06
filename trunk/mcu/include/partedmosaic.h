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

	virtual int Update(int index,BYTE *frame,int width,int heigth);
	virtual int Clean(int index);

private:
	int     mosaicCols;
	int     mosaicRows;
	int     mosaicNum;
	int     mosaicWidth;
	int     mosaicHeight;
};

#endif
