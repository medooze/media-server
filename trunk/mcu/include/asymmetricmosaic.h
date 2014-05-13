#ifndef _ASYMMETRICMOSAIC_H_
#define _ASYMMETRICMOSAIC_H_

#include "mosaic.h"
#include "framescaler.h"

class AsymmetricMosaic:
	public Mosaic
{
public:
	AsymmetricMosaic(Type type, DWORD size);
	virtual ~AsymmetricMosaic();

	virtual int Update(int index,BYTE *frame,int width,int heigth, bool keepAspectRatio);
	virtual int Clean(int index);

	virtual int GetWidth(int pos);
	virtual int GetHeight(int pos);
	virtual int GetTop(int pos);
	virtual int GetLeft(int pos);
};
#endif
