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
int AsymmetricMosaic::Update(int pos, BYTE *image, int imgWidth, int imgHeight,bool keepAspectRatio)
{
	//Check it's in the mosaic
	if (pos<0 || pos >= numSlots)
		return 0;
	
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
		resizer[pos]->SetResize(imgWidth,imgHeight,imgWidth,mosaicWidth,mosaicHeight,mosaicTotalWidth,keepAspectRatio);
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
	//Check it's in the mosaic
	if (pos<0 || pos >= numSlots)
		return 0;

	DWORD mosaicNumPixels = mosaicTotalWidth*mosaicTotalHeight;
	DWORD offset=0;
	DWORD offset2=0;
	BYTE *lineaY;
	BYTE *lineaU;
	BYTE *lineaV;

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
		memset(lineaY, 0, mosaicWidth);
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
		case mosaic1p4A:
			/**********************************************
			*	-----------------------
			*      | 	           | 2 |
			*      |                   |---|
			*      |                   | 3 |
			*      |         1         |---|
			*      |   	           | 4 |
			*      |                   |---|
			*      |   	           | 5 |
			*      |-----------------------|
			*
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			cols = 5;
			if(!pos)
				size = 4;
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
		case mosaic1p2A:
			/**********************************************
			*	-----------------
			*      |           |  2  |
			*      |     1     |---- |
			*      |	   |  3  |
			*      ------------------
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			cols = 3;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
		case mosaic1p2x2A:


			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |    1	  |--- |--- |
			*      |	  | 4  | 5  |
			*	--------------------
			***********************************************/
			cols = 4;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
		case mosaic1p6A:
			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |     	  |--- |--- |
			*      |    1     | 4  | 5  |
			*      |     	  |--- |--- |
			*      |	  | 6  | 7  |
			*	--------------------
			***********************************************/
			cols = 4;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
		case mosaic1p12:
			/**********************************************
			*	----------------
			*      |       | 2 | 3 | 
			*      |   1   |---|---|
			*      |       | 4 | 5 |
			*      |-------|---|---|
			*      | 6 | 7 | 8 | 9 |
			*      |-------|---|---|
			*      | 10| 11| 12| 13|
			*	----------------
			***********************************************/
			cols = 4;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
		case mosaic1p16A:
			/**********************************************
			*	--------------------
			*      |       | 2 | 3 | 4 |
			*      |   1   |---|---|---|
			*      |       | 5 | 6 | 7 |
			*      |-------|---|---|---|
			*      | 8 | 9 | 10| 11| 12|
			*      |-------|---|---|---|
			*      | 13| 14| 15| 16| 17|
			*	--------------------
			***********************************************/
			cols = 5;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
	}
	return (mosaicTotalWidth/cols)*size;
}
int AsymmetricMosaic::GetHeight(int pos)
{
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
		case mosaic1p4A:
			/**********************************************
			*	-----------------------
			*      | 	           | 2 |
			*      |                   |---|
			*      |                   | 3 |
			*      |         1         |---|
			*      |   	           | 4 |
			*      |                   |---|
			*      |   	           | 5 |
			*      |-----------------------|
			* 
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			rows = 4;
			if(!pos)
				size = 4;
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
		case mosaic1p2A:
			/**********************************************
			*	-----------------
			*      |           |  2  |
			*      |     1     |---- |
			*      |	   |  3  |
			*      ------------------
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			rows = 2;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
		case mosaic1p2x2A:


			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |    1	  |--- |--- |
			*      |	  | 4  | 5  |
			*	--------------------
			***********************************************/
			rows = 2;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
		case mosaic1p6A:
			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |     	  |--- |--- |
			*      |    1     | 4  | 5  |
			*      |     	  |--- |--- |
			*      |	  | 6  | 7  |
			*	--------------------
			***********************************************/
			rows = 3;
			if(!pos)
				size = 3;
			else
				size = 1;
			break;
		case mosaic1p12:
			/**********************************************
			*	----------------
			*      |       | 2 | 3 |
			*      |   1   |---|---|
			*      |       | 5 | 6 | 
			*      |-------|---|---|
			*      | 8 | 9 | 10| 11|
			*      |-------|---|---|
			*      | 13| 14| 15| 16|
			*	----------------
			***********************************************/
			rows = 4;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
		case mosaic1p16A:
			/**********************************************
			*	--------------------
			*      |       | 2 | 3 | 4 |
			*      |   1   |---|---|---|
			*      |       | 5 | 6 | 7 |
			*      |-------|---|---|---|
			*      | 8 | 9 | 10| 11| 12|
			*      |-------|---|---|---|
			*      | 13| 14| 15| 16| 17|
			*	--------------------
			***********************************************/
			rows = 4;
			if(!pos)
				size = 2;
			else
				size = 1;
			break;
	}
	return (mosaicTotalHeight/rows)*size;
}

int AsymmetricMosaic::GetTop(int pos)
{
	const BYTE* index;
	BYTE rows = 1;
	BYTE cols = 1;

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
			cols = 4;
			rows = 4;
			index = (BYTE[]){0,2,8,10,11,14,15};
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
			rows = 4;
			index = (BYTE[]){0,3,7,11,12,13,14,15};
			break;
		case mosaic1p4A:
			/**********************************************
			*	-----------------------
			*      | 	           | 2 |
			*      |                   |---|
			*      |                   | 3 |
			*      |         1         |---|
			*      |   	           | 4 |
			*      |                   |---|
			*      |   	           | 5 |
			*      |-----------------------|
			*
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			cols = 5;
			rows = 4;
			index = (BYTE[]){0,4,9,14,19};
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
			rows = 3;
			index = (BYTE[]){0,2,5,6,7,8};
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
		case mosaic1p2A:
			/**********************************************
			*	-----------------
			*      |           |  2  |
			*      |     1     |---- |
			*      |	   |  3  |
			*      ------------------
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			cols = 3;
			rows = 2;
			index = (BYTE[]){0,2,5};
			break;
		case mosaic1p2x2A:
			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |    1	  |--- |--- |
			*      |	  | 4  | 5  |
			*	--------------------
			***********************************************/
			cols = 4;
			rows = 2;
			index = (BYTE[]){0,2,3,6,7};
			break;
		case mosaic1p6A:
			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |     	  |--- |--- |
			*      |    1     | 4  | 5  |
			*      |     	  |--- |--- |
			*      |	  | 6  | 7  |
			*	--------------------
			***********************************************/
			cols = 4;
			rows = 3;
			index = (BYTE[]){0,2,3,6,7,10,11};
			break;
		case mosaic1p12:
			/**********************************************
			*	----------------
			*      |       | 2 | 3 |
			*      |   1   |---|---|
			*      |       | 4 | 5 |
			*      |-------|---|---|
			*      | 6 | 7 |  8| 9 |
			*      |-------|---|---|
			*      | 10| 11| 12| 13|
			*	----------------
			***********************************************/
			cols = 4;
			rows = 4;
			index = (BYTE[]){0, 2,3, 6,7, 8,9,10,11, 12,13,14,15};
			break;
		case mosaic1p16A:
			/**********************************************
			*	--------------------
			*      |       | 2 | 3 | 4 |
			*      |   1   |---|---|---|
			*      |       | 5 | 6 | 7 |
			*      |-------|---|---|---|
			*      | 8 | 9 | 10| 11| 12|
			*      |-------|---|---|---|
			*      | 13| 14| 15| 16| 17|
			*	--------------------
			***********************************************/
			cols = 5;
			rows = 4;
			index = (BYTE[]){0, 2,3,4, 7,8,9,  10,11,12,13,14, 15,16,17,18,19};
			break;
	}
	//Get row
	int i = index[pos]/cols;
	//Get col
	int j = index[pos] - i*cols;
	//Get row heigth
	int mosaicHeight = mosaicTotalHeight/rows;
	//Calculate top
	return mosaicHeight*i;
}

int AsymmetricMosaic::GetLeft(int pos)
{
	BYTE *index;
	BYTE rows = 1;
	BYTE cols = 1;

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
			cols = 4;
			rows = 4;
			index = (BYTE[]){0,2,8,10,11,14,15};
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
			rows = 4;
			index = (BYTE[]){0,3,7,11,12,13,14,15};
			break;
		case mosaic1p4A:
			/**********************************************
			*	-----------------------
			*      | 	           | 2 |
			*      |                   |---|
			*      |                   | 3 |
			*      |         1         |---|
			*      |   	           | 4 |
			*      |                   |---|
			*      |   	           | 5 |
			*      |-----------------------|
			*
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			cols = 5;
			rows = 4;
			index = (BYTE[]){0,4,9,14,19};
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
			rows = 3;
			index = (BYTE[]){0,2,5,6,7,8};
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
			cols = 2;
			rows = 1;
			index = (BYTE[]){0,1};
			break;
		case mosaic1p2A:
			/**********************************************
			*	-----------------
			*      |           |  2  |
			*      |     1     |---- |
			*      |	   |  3  |
			*      ------------------
			*      Participant size has to be different than mosaic size to keep aspect ratio
			***********************************************/
			cols = 3;
			rows = 2;
			index = (BYTE[]){0,2,5};
			break;
		case mosaic1p2x2A:
			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |    1	  |--- |--- |
			*      |	  | 4  | 5  |
			*	--------------------
			***********************************************/
			cols = 4;
			rows = 2;
			index = (BYTE[]){0,2,3,6,7};
			break;
		case mosaic1p6A:
			/**********************************************
			*	--------------------
			*      |	  | 2  | 3  |
			*      |     	  |--- |--- |
			*      |    1     | 4  | 5  |
			*      |     	  |--- |--- |
			*      |	  | 6  | 7  |
			*	--------------------
			***********************************************/
			cols = 4;
			rows = 3;
			index = (BYTE[]){0,2,3,6,7,10,11};
			break;
		case mosaic1p12:
			/**********************************************
			*	----------------
			*      |       | 2 | 3 |
			*      |   1   |---|---|
			*      |       | 4 | 5 |
			*      |-------|---|---|
			*      | 6 | 7 |  8| 9 |
			*      |-------|---|---|
			*      | 10| 11| 12| 13|
			*	----------------
			***********************************************/
			cols = 4;
			rows = 4;
			index = (BYTE[]){0, 2,3, 6,7, 8,9,10,11, 12,13,14,15};
			break;
		case mosaic1p16A:
			/**********************************************
			*	--------------------
			*      |       | 2 | 3 | 4 |
			*      |   1   |---|---|---|
			*      |       | 5 | 6 | 7 |
			*      |-------|---|---|---|
			*      | 8 | 9 | 10| 11| 12|
			*      |-------|---|---|---|
			*      | 13| 14| 15| 16| 17|
			*	--------------------
			***********************************************/
			cols = 5;
			rows = 4;
			index = (BYTE[]){0, 2,3,4, 7,8,9,  10,11,12,13,14, 15,16,17,18,19};
			break;
	}
	//Get row
	int i = index[pos]/cols;
	//Get col
	int j = index[pos] - i*cols;
	int mosaicWidth = mosaicTotalWidth/cols;
	//Start filling from the end to not cause overlap
	return mosaicWidth*j;
}