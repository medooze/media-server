#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "log.h"
#include "pipmosaic.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
}

/***********************
* PIPMosaic
*	Constructor
************************/
PIPMosaic::PIPMosaic(Mosaic::Type type, DWORD size) : Mosaic(type,size)
{
	//Get width and height
	mosaicTotalWidth = ::GetWidth(size);
	mosaicTotalHeight = ::GetHeight(size);

	//Allocate memory for under image
	underBuffer = (BYTE *) malloc32(mosaicSize);
	//Get aligned
	under = ALIGNTO32(underBuffer);
	//Reset mosaic
	memset(underBuffer ,(BYTE)-128,mosaicSize);
}

/***********************
* PIPMosaic
*	Destructor
************************/
PIPMosaic::~PIPMosaic()
{
	//Free memory
	free(underBuffer);
}

/*****************************
* Update
* 	Update slot of mosaic with given image
*****************************/
int PIPMosaic::Update(int pos, BYTE *image, int imgWidth, int imgHeight,bool keepAspectRatio)
{
	//Check size
	if (!image && !imgHeight && !imgHeight)
	{
		//Clean position
		Clean(pos);
		//Exit
		return 0;

	}
	//Check it's in the mosaic
	if (pos+1>numSlots)
		//Exit
		return 0;

	//Get plane pointers from destination
	DWORD numSlotsPixels = mosaicTotalWidth*mosaicTotalHeight;
	BYTE *lineaY = mosaic;
	BYTE *lineaU = mosaic + numSlotsPixels;
	BYTE *lineaV = lineaU + numSlotsPixels/4;
	//Get plane pointers from incoming image
	DWORD imgNumPixels = imgHeight*imgWidth;
	BYTE *imageY = image;
	BYTE *imageU = imageY + imgNumPixels;
	BYTE *imageV = imageU + imgNumPixels/4;


	/**********************************************
	*	--------------------
	*      |           	    |
	*      |     	 1          |
	*      |                    |
	*      | -----  ----- ----- |
	*      | | 2 |  | 3 | | 4 | |
	*      | -----  ----- ----- |
	*	--------------------
	***********************************************/

	//Calculate pip participant size
	DWORD mosaicWidth		= SIZE4MUL(mosaicTotalWidth/5);
	DWORD mosaicHeight		= SIZE4MUL(mosaicTotalHeight/5);
	DWORD mosaicTotalWidthUV	= mosaicTotalWidth/2;
	DWORD mosaicWidthUV		= mosaicWidth/2;
	//Get empty space between PIP
	DWORD intraWidth	= mosaicWidth/2;
	DWORD intraWidthUV	= intraWidth/2;
	DWORD intraHeight	= mosaicHeight/4;
	
	//If only 2 PIP
	if (numSlots==2) 
	{
		/**********************************************
		*	--------------------
		*      |           	    |
		*      |     	 1          |
		*      | -------            |
		*      | |     |            |
		*      | |  2  |            |
		*      | -------         -  |
		*	--------------------
		***********************************************/
		//Change size
		mosaicWidth		= SIZE4MUL(mosaicTotalWidth/4);
		mosaicHeight		= SIZE4MUL(mosaicTotalHeight/4);
	}
	//Get top position
	DWORD pipIni			= SIZE4MUL(mosaicTotalHeight-mosaicHeight-intraHeight);
	//Get pip ini
	DWORD pipYOffset  = mosaicTotalWidth*pipIni;
	DWORD pipUVOffset = pipYOffset/4;

	//Check which position is it
	if (pos==0)
	{
		//Check if the sizes are equal
		if ((imgWidth!=mosaicTotalWidth) || (imgHeight!=mosaicTotalHeight))
		{
			//Get pointers of the under image
			BYTE *underY = under;
			BYTE *underU = under  + numSlotsPixels;
			BYTE *underV = underU + numSlotsPixels/4;

			//Set resize
			resizer[pos]->SetResize(imgWidth,imgHeight,imgWidth,mosaicTotalWidth,mosaicTotalHeight,mosaicTotalWidth,keepAspectRatio);
			//Resize and set to slot
			resizer[pos]->Resize(imageY,imageU,imageV,underY,underU,underV);

			//Change pointers from image to resized one
			imageY = underY;
			imageU = underU;
			imageV = underV;
		}

		//Copy to the begining of the pip zone
		for (int j=0; j<pipIni/2; ++j)
		{
			//Copy Y line
			memcpy(lineaY, imageY, mosaicTotalWidth);
			//Go to next
			lineaY += mosaicTotalWidth;
			imageY += mosaicTotalWidth;

			//Copy Y line
			memcpy(lineaY, imageY, mosaicTotalWidth);
			//Go to next
			lineaY += mosaicTotalWidth;
			imageY += mosaicTotalWidth;

			//Copy U and V lines
			memcpy(lineaU, imageU, mosaicTotalWidthUV);
			memcpy(lineaV, imageV, mosaicTotalWidthUV);
			//Go to next
			lineaU += mosaicTotalWidthUV;
			lineaV += mosaicTotalWidthUV;
			imageU += mosaicTotalWidthUV;
			imageV += mosaicTotalWidthUV;
		}

		//Copy in the pip area
		for (int j=0; j<mosaicHeight/2; ++j)
		{
			DWORD pipY  = 0;
			DWORD pipUV = 0;
			//Copy skipping pip images
			for (int i=0;i<numSlots-1;++i)
			{
				//Copy Y lines
				memcpy(lineaY + pipY			, imageY + pipY				, intraWidth);
				memcpy(lineaY + pipY + mosaicTotalWidth	, imageY + pipY + mosaicTotalWidth	, intraWidth);
				//skip next
				pipY += intraWidth+mosaicWidth;
				//Copy U and V lines
				memcpy(lineaU + pipUV , imageU + pipUV , intraWidthUV);
				memcpy(lineaV + pipUV , imageV + pipUV , intraWidthUV);
				//Go to next
				pipUV += intraWidthUV+mosaicWidthUV;
			}
			//Copy Y lines until the end
			memcpy(lineaY + pipY				, imageY + pipY				, mosaicTotalWidth-pipY);
			memcpy(lineaY + pipY + mosaicTotalWidth		, imageY + pipY + mosaicTotalWidth	, mosaicTotalWidth-pipY);
			//Copy U and V lines until the end
			memcpy(lineaU + pipUV, imageU + pipUV, mosaicTotalWidthUV-pipUV);
			memcpy(lineaV + pipUV, imageV + pipUV, mosaicTotalWidthUV-pipUV);
			//Go to next line
			lineaY += mosaicTotalWidth;
			imageY += mosaicTotalWidth;
			lineaY += mosaicTotalWidth;
			imageY += mosaicTotalWidth;
			lineaU += mosaicTotalWidthUV;
			lineaV += mosaicTotalWidthUV;
			imageU += mosaicTotalWidthUV;
			imageV += mosaicTotalWidthUV;
		}
		//Copy from the end pip zone
		for (int j=(pipIni+mosaicHeight)/2; j<mosaicTotalHeight/2; ++j)
		{
			//Copy Y line
			memcpy(lineaY, imageY, mosaicTotalWidth);
			//Go to next
			lineaY += mosaicTotalWidth;
			imageY += mosaicTotalWidth;

			//Copy Y line
			memcpy(lineaY, imageY, mosaicTotalWidth);
			//Go to next
			lineaY += mosaicTotalWidth;
			imageY += mosaicTotalWidth;

			//Copy U and V lines
			memcpy(lineaU, imageU, mosaicTotalWidthUV);
			memcpy(lineaV, imageV, mosaicTotalWidthUV);
			//Go to next
			lineaU += mosaicTotalWidthUV;
			lineaV += mosaicTotalWidthUV;
			imageU += mosaicTotalWidthUV;
			imageV += mosaicTotalWidthUV;
		}
	} else {
		//Get offsets
		DWORD offsetY  = pipYOffset  + intraWidth   + (intraWidth+mosaicWidth)*(pos-1);
		DWORD offsetUV = pipUVOffset + intraWidthUV + (intraWidthUV+mosaicWidthUV)*(pos-1);
		//Get plane pointers
		lineaY += offsetY;
		lineaU += offsetUV;
		lineaV += offsetUV;

		//Check if the sizes are equal
		if ((imgWidth == mosaicWidth) && (imgHeight == mosaicHeight))
		{
			//Copy Y plane
			for (int i = 0; i<imgHeight; i++)
			{
				//Copy Y line
				memcpy(lineaY, imageY, mosaicWidth);

				//Go to next
				lineaY += mosaicTotalWidth;
				imageY += imgWidth;
			}

			//Copy U and V planes
			for (int i = 0; i<imgHeight/2; i++)
			{
				//Copy U and V lines
				memcpy(lineaU, imageU, mosaicWidthUV);
				memcpy(lineaV, imageV, mosaicWidthUV);

				//Go to next
				lineaU += mosaicTotalWidthUV;
				lineaV += mosaicTotalWidthUV;

				imageU += mosaicWidthUV;
				imageV += mosaicWidthUV;
			}
		} else if ((imgWidth > 0) && (imgHeight > 0)) {
			//Set resize
			resizer[pos]->SetResize(imgWidth,imgHeight,imgWidth,mosaicWidth,mosaicHeight,mosaicTotalWidth,keepAspectRatio);
			//Resize and set to slot
			resizer[pos]->Resize(imageY,imageU,imageV,lineaY,lineaU,lineaV);
		}
	}

	//We have changed
	SetChanged();

	return 1;
}

/*****************************
* Clear
* 	Clear slot imate
*****************************/
int PIPMosaic::Clean(int pos)
{
}


/***********************
* GetWidth
*	Get mosaic width
************************/
int PIPMosaic::GetWidth()
{
	return mosaicTotalWidth;
}

/***********************
* GetHeight
*	Get mosaic height
************************/
int PIPMosaic::GetHeight()
{
	return mosaicTotalHeight;
}

/***********************
* GetFrame
*	Get mosaic frame
************************/
BYTE* PIPMosaic::GetFrame()
{
	return mosaic;
}

/**********************
* GetSlots
*	Get number of slots
***********************/
int PIPMosaic::GetSlots()
{
	return numSlots+1;
}
int PIPMosaic::GetWidth(int pos)
{
	//Check it's in the mosaic
	if (pos+1>numSlots)
		//Exit
		return 0;
	//Main
	if (!pos)
		return mosaicTotalWidth;

	//If only 2 PIP
	if (numSlots==2)
		return SIZE4MUL(mosaicTotalWidth/4);

	return SIZE4MUL(mosaicTotalWidth/5);
}

int PIPMosaic::GetHeight(int pos)
{
	//Check it's in the mosaic
	if (pos+1>numSlots)
		//Exit
		return 0;
	//Main
	if(!pos)
		return mosaicTotalHeight;
	//If only 2 PIP
	if (numSlots==2)
		return SIZE4MUL(mosaicTotalHeight/4);
	return SIZE4MUL(mosaicTotalHeight/5);
}
int PIPMosaic::GetTop(int pos)
{
	//Check it's in the mosaic
	if (pos+1>numSlots)
		//Exit
		return 0;
	//Main
	if (!pos)
		return 0;
	//Calculate pip participant size
	DWORD mosaicHeight		= SIZE4MUL(mosaicTotalHeight/5);
	//Get top position
	return SIZE4MUL(mosaicTotalHeight-mosaicHeight-mosaicHeight/2);
}
int PIPMosaic::GetLeft(int pos)
{
	//Check it's in the mosaic
	if (pos+1>numSlots)
		//Exit
		return 0;
		//Main
	if (!pos)
		return 0;
	//Calculate pip participant size
	DWORD mosaicWidth		= SIZE4MUL(mosaicTotalWidth/5);
	//Get empty space between PIP
	DWORD intraWidth = mosaicWidth/2;

	return intraWidth + (intraWidth+mosaicWidth)*(pos-1);
}