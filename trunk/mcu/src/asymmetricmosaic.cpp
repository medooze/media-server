#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "log.h"
#include "asymmetricmosaic.h"

/***********************
* AsymmetricMosaic
*	Constructor
************************/
AsymmetricMosaic::AsymmetricMosaic(Mosaic::Type type, DWORD size) : Mosaic(type,size)
{
}

/***********************
* AsymmetricMosaic
*	Destructor
************************/
AsymmetricMosaic::~AsymmetricMosaic()
{
}

/*****************************
* Update
* 	Update slot of mosaic with given image
*****************************/
int AsymmetricMosaic::Update(int pos, BYTE *image, int imgWidth, int imgHeight)
{
	DWORD mosaicNumPixels = mosaicTotalWidth*mosaicTotalHeight;
	DWORD offset=0;
	DWORD offset2=0;
	BYTE *lineaY;
	BYTE *lineaU;
	BYTE *lineaV;
	
	DWORD imgNumPixels = imgHeight*imgWidth;
	BYTE *imageY = image;
	BYTE *imageU  = imageY  + imgNumPixels;
	BYTE *imageV  = imageU + imgNumPixels/4;

	int i;
	int j;
	int index;
	int mosaicWidth;
	int mosaicHeight;
	int cols;

	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	//Get values
	switch(mosaicType)
	{
		case mosaic3p4:
			/**********************************************
			*	--------------------
			*      |          |	    |
			*      |    1	  |    2    |	
			*      |_________ |_________|
			*      |	  | 4  | 5  |
			*      |    3	  |--- |--- |
			*      |	  | 6  | 7  |
			*	--------------------
			***********************************************/
			switch(pos)
			{
				case 3:
					index = 10;
					cols = 4;
					break;
				case 4:
					index = 11;
					cols = 4;
					break;
				case 5:
					index = 14;
					cols = 4;
					break;
				case 6:	
					index = 15;
					cols = 4;
					break;
				default:
					index = pos;
					cols = 2;
			}

			i = index/cols;
			j = index - i*cols;
			mosaicWidth = (int)mosaicTotalWidth/cols;
			mosaicHeight = (int)mosaicTotalHeight/cols;
			break;
		case mosaic1p7:
			/**********************************************
			*	----------------
			*      |	    | 2 |
			*      |            |---|
			*      |     1      | 3 |	
			*      |            |---|
			*      |	    | 4 |
			*      |------------|---|
			*      | 5 | 6	| 7 | 8 |
			*	----------------
			***********************************************/
			switch(pos)
			{
				case 0:
					index = 0;
					i = 0;
					j = 0;
					mosaicWidth = (int)(mosaicTotalWidth*3/4);
					mosaicHeight = (int)(mosaicTotalHeight*3/4);
					break;
				case 1:
					index = 3;
					break;
				case 2:
					index = 7;
					break;
				case 3:
					index = 11;
					break;
				case 4:
					index = 12;
					break;
				case 5:
					index = 13;
					break;
				case 6:
					index = 14;
					break;
				case 7:
					index = 15;
					break;
			}
		
			if (index)
			{
				i = index/4;
				j = index - i*4;
				mosaicWidth = (int)mosaicTotalWidth/4;
				mosaicHeight = (int)mosaicTotalHeight/4;
			}
			break;
		case mosaic1p5:
			/**********************************************
			*	-----------------
			*      |           |  2  |	
			*      |     1     |---- |
			*      |	   |  3  |
			*      |---------- |---- |
			*      |  4  |  5  |  6  |
			*	------------------
			***********************************************/
			switch (pos)
			{
				case 0:
					index = 0;
					i = 0;
					j = 0;
					mosaicWidth = (int)(mosaicTotalWidth*2/3);
					mosaicHeight = (int)(mosaicTotalHeight*2/3);
					break;
				case 1:
					index = 2;
					break;			
				case 2:
					index = 5;
					break;			
				case 3:
					index = 6;
					break;			
				case 4:
					index = 7;
					break;			
				case 5:
					index = 8;
					break;			
			}

			if (index)
			{
				i = index/3;
				j = index - i*3;
				mosaicWidth = (int)mosaicTotalWidth/3;
				mosaicHeight = (int)mosaicTotalHeight/3;
			}
			break;
		case mosaic1p1:
			/**********************************************
			*	----------------
			*      |	        |
			*      |----------------|
			*      |       |        |
			*      |   1   |   2    |
			*      |       |        |
			*      |----------------|
			*      |      	        |
			*	----------------
			***********************************************/
			switch(pos)
			{
				case 0:
					i = 0;
					j = 0;
					mosaicWidth = (int)(mosaicTotalWidth/2);
					mosaicHeight = (int)(mosaicTotalHeight/2);
					break;
				case 1:
					i = 0;
					j = 1;
					mosaicWidth = (int)(mosaicTotalWidth/2);
					mosaicHeight = (int)(mosaicTotalHeight/2);
					break;
			}
			//Vertical center
			offset = (mosaicTotalWidth*mosaicHeight/2);
			offset2 = (mosaicTotalWidth*mosaicHeight)/8;
			break;
	}

	//Get offsets
	offset += (mosaicTotalWidth*mosaicHeight*i) + mosaicWidth*j;
	offset2 += (mosaicTotalWidth*mosaicHeight*i)/4+(mosaicWidth*j)/2;

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
	} else if ((imgWidth > 0) && (imgHeight > 0)) {
		//Set resize
		resizer[pos]->SetResize(imgWidth,imgHeight,imgWidth,mosaicWidth,mosaicHeight,mosaicTotalWidth);
		//Resize and set to slot
		resizer[pos]->Resize(imageY,imageU,imageV,lineaY,lineaU,lineaV);
	}

	//We have changed
	SetChanged();

	return 1;
}

/*****************************
* Clear
* 	Clear slot imate
*****************************/
int AsymmetricMosaic::Clean(int pos)
{
	DWORD mosaicNumPixels = mosaicTotalWidth*mosaicTotalHeight;
	DWORD offset=0;
	DWORD offset2=0;
	BYTE *lineaY;
	BYTE *lineaU;
	BYTE *lineaV;

	int i;
	int j;
	int index;
	int mosaicWidth;
	int mosaicHeight;
	int cols;

	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	//Get values
	switch(mosaicType)
	{
		case mosaic3p4:
			/**********************************************
			*	--------------------
			*      |          |	    |
			*      |    1	  |    2    |
			*      |_________ |_________|
			*      |	  | 4  | 5  |
			*      |    3	  |--- |--- |
			*      |	  | 6  | 7  |
			*	--------------------
			***********************************************/
			switch(pos)
			{
				case 3:
					index = 10;
					cols = 4;
					break;
				case 4:
					index = 11;
					cols = 4;
					break;
				case 5:
					index = 14;
					cols = 4;
					break;
				case 6:
					index = 15;
					cols = 4;
					break;
				default:
					index = pos;
					cols = 2;
			}

			i = index/cols;
			j = index - i*cols;
			mosaicWidth = (int)mosaicTotalWidth/cols;
			mosaicHeight = (int)mosaicTotalHeight/cols;
			break;
		case mosaic1p7:
			/**********************************************
			*	----------------
			*      |	    | 2 |
			*      |            |---|
			*      |     1      | 3 |
			*      |            |---|
			*      |	    | 4 |
			*      |------------|---|
			*      | 5 | 6	| 7 | 8 |
			*	----------------
			***********************************************/
			switch(pos)
			{
				case 0:
					index = 0;
					i = 0;
					j = 0;
					mosaicWidth = (int)(mosaicTotalWidth*3/4);
					mosaicHeight = (int)(mosaicTotalHeight*3/4);
					break;
				case 1:
					index = 3;
					break;
				case 2:
					index = 7;
					break;
				case 3:
					index = 11;
					break;
				case 4:
					index = 12;
					break;
				case 5:
					index = 13;
					break;
				case 6:
					index = 14;
					break;
				case 7:
					index = 15;
					break;
			}

			if (index)
			{
				i = index/4;
				j = index - i*4;
				mosaicWidth = (int)mosaicTotalWidth/4;
				mosaicHeight = (int)mosaicTotalHeight/4;
			}
			break;
		case mosaic1p5:
			/**********************************************
			*	-----------------
			*      |           |  2  |
			*      |     1     |---- |
			*      |	   |  3  |
			*      |---------- |---- |
			*      |  4  |  5  |  6  |
			*	------------------
			***********************************************/
			switch (pos)
			{
				case 0:
					index = 0;
					i = 0;
					j = 0;
					mosaicWidth = (int)(mosaicTotalWidth*2/3);
					mosaicHeight = (int)(mosaicTotalHeight*2/3);
					break;
				case 1:
					index = 2;
					break;
				case 2:
					index = 5;
					break;
				case 3:
					index = 6;
					break;
				case 4:
					index = 7;
					break;
				case 5:
					index = 8;
					break;
			}

			if (index)
			{
				i = index/3;
				j = index - i*3;
				mosaicWidth = (int)mosaicTotalWidth/3;
				mosaicHeight = (int)mosaicTotalHeight/3;
			}
			break;
		case mosaic1p1:
			/**********************************************
			*	----------------
			*      |	        |
			*      |----------------|
			*      |       |        |
			*      |   1   |   2    |
			*      |       |        |
			*      |----------------|
			*      |      	        |
			*	----------------
			***********************************************/
			switch(pos)
			{
				case 0:
					i = 0;
					j = 0;
					mosaicWidth = (int)(mosaicTotalWidth/2);
					mosaicHeight = (int)(mosaicTotalHeight/2);
					break;
				case 1:
					i = 0;
					j = 1;
					mosaicWidth = (int)(mosaicTotalWidth/2);
					mosaicHeight = (int)(mosaicTotalHeight/2);
					break;
			}
			//Vertical center
			offset = (mosaicTotalWidth*mosaicHeight/2);
			offset2 = (mosaicTotalWidth*mosaicHeight)/8;
			break;
	}

	//Get offsets
	offset += (mosaicTotalWidth*mosaicHeight*i) + mosaicWidth*j;
	offset2 += (mosaicTotalWidth*mosaicHeight*i)/4+(mosaicWidth*j)/2;

	//Get plane pointers
	lineaY = mosaic + offset;
	lineaU = mosaic + mosaicNumPixels + offset2;
	lineaV = lineaU + mosaicNumPixels/4;

	//Clear Y plane
	for (int i = 0; i<mosaicHeight; i++)
	{
		//Copy Y line
		memset(lineaY, (BYTE)-128, mosaicWidth);
		//Go to next
		lineaY += mosaicTotalWidth;
	}

	//Clear U and V planes
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
