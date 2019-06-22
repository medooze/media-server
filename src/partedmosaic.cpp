#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "log.h"
#include "partedmosaic.h"
#include <stdexcept>

/***********************
* PartedMosaic
*	Constructor
************************/
PartedMosaic::PartedMosaic(Type type, DWORD size) : Mosaic(type,size)
{
	//Divide mosaic
	switch(type)
	{
		case mosaic1x1:
			//Set num rows and cols
			mosaicCols = 1;
			mosaicRows = 1;
			break;
		case mosaic2x2:
			//Set num rows and cols
			mosaicCols = 2;
			mosaicRows = 2;
			break;
		case mosaic3x3:
			//Set num rows and cols
			mosaicCols = 3;
			mosaicRows = 3;
			break;
		case mosaic4x4:
			//Set num rows and cols
			mosaicCols = 4;
			mosaicRows = 4;
			break;
		case mosaic4x5A:
			//Set num rows and cols
			mosaicCols = 4;
			mosaicRows = 5;
			break;
		case mosaic5x5:
			//Set num rows and cols
			mosaicCols = 5;
			mosaicRows = 5;
			break;
		case mosaic1p1A:
			//Set num rows and cols
			mosaicCols = 2;
			mosaicRows = 1;
			break;
		default:
			throw new std::runtime_error("Unknown mosaic type\n");

	}
	mosaicWidth = SIZE2MUL((int)mosaicTotalWidth/mosaicCols);
	mosaicHeight = SIZE2MUL((int)mosaicTotalHeight/mosaicRows);
}

/***********************
* PartedMosaic
*	Destructor
************************/
PartedMosaic::~PartedMosaic()
{
}

/*****************************
* Update
* 	Update slot of mosaic with given image
*****************************/
int PartedMosaic::Update(int pos, BYTE *image, int imgWidth, int imgHeight,bool keepAspectRatio)
{
	//Check it's in the mosaic
	if (pos<0 || pos >= numSlots)
		return 0;

	//Check size
	if (!image && !imgWidth && !imgHeight)
	{
		//Clean position
		Clean(pos);
		//Exit
		return 0;

	}

	DWORD mosaicNumPixels = mosaicTotalWidth*mosaicTotalHeight;
	DWORD offset,offset2;
	BYTE *lineaY;
	BYTE *lineaU;
	BYTE *lineaV;
	
	DWORD imgNumPixels = imgHeight*imgWidth;
	BYTE *imageY = image;
	BYTE *imageU  = image  + imgNumPixels;
	BYTE *imageV  = imageU + imgNumPixels/4;

	//Get slot position in mosaic
	int i = pos / mosaicCols;
	int j = pos - i*mosaicCols;

	//Get offsets
	offset = (mosaicTotalWidth*mosaicHeight*i) + mosaicWidth*j; 
	offset2 = (mosaicTotalWidth*mosaicHeight/4)*i+(mosaicWidth/2)*j;

	//Get plane pointers
	lineaY = mosaic + offset;
	lineaU = mosaic + mosaicNumPixels + offset2;
	lineaV = lineaU + mosaicNumPixels/4; 

	//Check if the sizes are equal 
	if ((imgWidth == mosaicWidth) && (imgHeight == mosaicHeight))
	{
		//Copy Y plane
		for (int i = 0; i<imgHeight; i++) 
		{
			//Copy Y line
			memcpy(lineaY, imageY, imgWidth);

			//Go to next
			lineaY += mosaicTotalWidth;
			imageY += imgWidth;
		}

		//Copy U and V planes
		for (int i = 0; i<imgHeight/2; i++) 
		{
			//Copy U and V lines
			memcpy(lineaU, imageU, imgWidth/2);
			memcpy(lineaV, imageV, imgWidth/2);

			//Go to next
			lineaU += mosaicTotalWidth/2;
			lineaV += mosaicTotalWidth/2;

			imageU += imgWidth/2;
			imageV += imgWidth/2;
		}
	} else {
		//Set resize
		resizer[pos]->SetResize(imgWidth,imgHeight,imgWidth,mosaicWidth,mosaicHeight,mosaicTotalWidth,keepAspectRatio);

		//And resize
		resizer[pos]->Resize(imageY,imageU,imageV,lineaY,lineaU,lineaV);
	}

	//We have changed
	SetChanged();

	return 1;
}

/*****************************
* Clean
* 	Clean slot image
*****************************/
int PartedMosaic::Clean(int pos)
{
	//Check it's in the mosaic
	if (pos<0 || pos >= numSlots)
		return 0;

	DWORD mosaicNumPixels = mosaicTotalWidth*mosaicTotalHeight;
	DWORD offset,offset2;
	BYTE *lineaY;
	BYTE *lineaU;
	BYTE *lineaV;

	//Get slot position in mosaic
	int i = pos / mosaicCols;
	int j = pos - i*mosaicCols;

	//Get offsets
	offset = (mosaicTotalWidth*mosaicHeight*i) + mosaicWidth*j;
	offset2 = (mosaicTotalWidth*mosaicHeight/4)*i+(mosaicWidth/2)*j;

	//Get plane pointers
	lineaY = mosaic + offset;
	lineaU = mosaic + mosaicNumPixels + offset2;
	lineaV = lineaU + mosaicNumPixels/4;

	//Copy Y plane
	for (int i = 0; i<mosaicHeight; i++)
	{
		//Copy Y line
		memset(lineaY, 0, mosaicWidth);
		//Go to next
		lineaY += mosaicTotalWidth;
	}

	//Copy U and V planes
	for (int i = 0; i<mosaicHeight/2; i++)
	{
		//Copy U and V lines
		memset(lineaU, (BYTE)-128, mosaicWidth/2);
		memset(lineaV, (BYTE)-128, mosaicWidth/2);

		//Go to next
		lineaU += mosaicTotalWidth/2;
		lineaV += mosaicTotalWidth/2;
	}

	//We have changed
	SetChanged();

	return 1;
}

int PartedMosaic::GetWidth(int pos)
{
	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	//Get widht
	return SIZE2MUL(mosaicWidth);
}
int PartedMosaic::GetHeight(int pos)
{
	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	//Get widht
	return SIZE2MUL(mosaicHeight);
}
int PartedMosaic::GetTop(int pos)
{
	//Get slot position in mosaic
	int i = pos / mosaicCols;
	int j = pos - i*mosaicCols;

	//Get offsets
	return SIZE2MUL(mosaicHeight*i);
}
int PartedMosaic::GetLeft(int pos)
{
	//Get slot position in mosaic
	int i = pos / mosaicCols;
	int j = pos - i*mosaicCols;
	//Get offsets
	return SIZE2MUL(mosaicWidth*j);
}
