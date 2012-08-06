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

	virtual int Update(int index,BYTE *frame,int width,int heigth);
	virtual int Clean(int index);
};
#endif
