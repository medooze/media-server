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
	//Check size
	if (!image && !imgHeight && !imgHeight)
		//Clean position
		return Clean(pos);

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

	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	//Get positions
	int left = GetLeft(pos);
	int top = GetTop(pos);
	int mosaicWidth = GetWidth(pos);
	int mosaicHeight = GetHeight(pos);

	//Get offsets
	offset += (mosaicTotalWidth*top) + left;
	offset2 += (mosaicTotalWidth*top)/4+left/2;

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
	} else {
		return 0;
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

	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;
	
	//Get positions
	int left = GetLeft(pos);
	int top = GetTop(pos);
	int mosaicWidth = GetWidth(pos);
	int mosaicHeight = GetHeight(pos);

	//Get offsets
	offset += (mosaicTotalWidth*top) + left;
	offset2 += (mosaicTotalWidth*top)/4+left/2;

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
int AsymmetricMosaic::GetWidth(int pos)
{
	
	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	BYTE cols = 1;
	BYTE size = 1;
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
			cols = 4;
			if (pos<3)
				size = 2;
			else
				size = 1;
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
			cols = 4;
			if(!pos)
				size = 3;
			else
				size = 1;
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
			cols = 3;
			if(!pos)
				size = 2;
			else
				size = 1;
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
			cols = 4;
			size = 2;
			break;
	}
	return (mosaicTotalWidth/cols)*size;
}
int AsymmetricMosaic::GetHeight(int pos)
{
	DWORD cols;
	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	BYTE rows = 1;
	BYTE size = 1;
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
			rows = 4;
			if (pos<3)
				size = 2;
			else
				size = 1;
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
			rows = 4;
			if(!pos)
				size = 3;
			else
				size = 1;
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
			rows = 3;
			if(!pos)
				size = 2;
			else
				size = 1;
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
			rows = 4;
			size = 2;
			break;
	}
	return (mosaicTotalHeight/rows)*size;
}

int AsymmetricMosaic::GetTop(int pos)
{
	BYTE index = 0;
	BYTE rows = 1;

	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

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
				case 0:
				case 1:
					return 0;
				case 2:
					index = 8;
				case 3:
					index = 10;
					break;
				case 4:
					index = 11;
					break;
				case 5:
					index = 14;
					break;
				case 6:
					index = 15;
					break;
			}
			rows = 4;
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
				case 1:
					return 0;
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
			rows = 4;
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
				case 1:
					return 0;
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
			rows = 3;
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
			//Alling vertically
			return mosaicTotalHeight/4;
	}
	//Get row
	int i = index/rows;
	//Get row heigth
	int mosaicHeight = mosaicTotalHeight/rows;
	//Calculate top
	return mosaicHeight*i;
}
int AsymmetricMosaic::GetLeft(int pos)
{
	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	//Check it's in the mosaic
	if (pos >= numSlots)
		return 0;

	BYTE index;
	BYTE cols;
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
				case 0:
				case 2:
					return 0;
				case 1:
					index = 2;
					break;
				case 3:
					index = 10;
					break;
				case 4:
					index = 11;
					break;
				case 5:
					index = 14;
					break;
				case 6:
					index = 15;
					break;
			}
			cols = 4;
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
					return 0;
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
			cols = 4;
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
					return 0;
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
			cols = 3;
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
			if(!pos)
					return 0;
			index = 1;
			cols = 2;
			break;
	}

	int i = index/cols;
	int j = index - i*cols;
	int mosaicWidth = mosaicTotalWidth/cols;
	//Start filling from the end to not cause overlap
	return mosaicWidth*j;
}