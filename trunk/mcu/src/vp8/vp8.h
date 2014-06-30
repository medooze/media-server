/* 
 * File:   vp8.h
 * Author: Sergio
 *
 * Created on 15 de noviembre de 2012, 15:06
 */

#ifndef _VP8_H_
#define	_VP8_H_
#include <string.h>
#include "config.h"
#include "log.h"

struct VP8PayloadDescriptor
{
	bool extendedControlBitsPresent;
	bool nonReferencePicture;
	bool startOfPartition;
	BYTE partitionIndex;
	bool pictureIdPresent;
	bool temporalLevelZeroIndexPresent;
	bool temporalLayerIndexPreset;
	bool keyIndexPresent;
	bool layerSync;
	WORD pictureId;
	BYTE temporalLevelZeroIndex;
	BYTE temporalLayerIndex;
	BYTE keyIndex;

	VP8PayloadDescriptor()
	{
		//Empty
		extendedControlBitsPresent = 0;
		nonReferencePicture = 0;
		startOfPartition = 0;
		partitionIndex = 0;
		pictureIdPresent = 0;
		temporalLevelZeroIndexPresent = 0;
		temporalLayerIndexPreset = 0;
		keyIndexPresent = 0;
		layerSync = 0;
		pictureId = 0;
		temporalLevelZeroIndex = 0;
		temporalLayerIndex = 0;
		keyIndex = 0;
	}
	
	VP8PayloadDescriptor(bool startOfPartition,BYTE partitionIndex)
	{
		//Empty
		extendedControlBitsPresent = 0;
		nonReferencePicture = 0;
		startOfPartition = 0;
		partitionIndex = 0;
		pictureIdPresent = 0;
		temporalLevelZeroIndexPresent = 0;
		temporalLayerIndexPreset = 0;
		keyIndexPresent = 0;
		layerSync = 0;
		pictureId = 0;
		temporalLevelZeroIndex = 0;
		temporalLayerIndex = 0;
		keyIndex = 0;
		//Set values
		this->startOfPartition = startOfPartition;
		this->partitionIndex = partitionIndex;
	}

	DWORD GetSize()
	{
		//Base size
		DWORD len = 1;
		if (extendedControlBitsPresent)
		{
			len++;
			if (pictureIdPresent)
			{
				if (pictureId>7)
					len+=2;
				else
					len+=1;
			}
			if (temporalLevelZeroIndexPresent)
				len++;
			if (temporalLayerIndexPreset || keyIndexPresent)
				len++;
		}
		return len;
	}

	DWORD Parse(BYTE* data, DWORD size)
	{
		DWORD len = 1;
		//Read first
		extendedControlBitsPresent	= data[0] >> 7;
		nonReferencePicture		= data[0] >> 5 & 0x01;
		startOfPartition		= data[0] >> 4 & 0x01;
		partitionIndex			= data[0] & 0x0F;

		//check if more
		if (extendedControlBitsPresent)
		{
			//Read second
			pictureIdPresent		= data[1] >> 7;
			temporalLevelZeroIndexPresent	= data[1] >> 6 & 0x01;
			temporalLayerIndex		= data[1] >> 5 & 0x01;
			keyIndex			= data[1] >> 4 & 0x01;
			//Increase len
			len++;

			//Check if we need to read picture id
			if (pictureIdPresent)
			{
				//Check mark
				if (data[len] & 0x80)
				{
					//Two bytes
					pictureId = (data[len] & 0x7F) << 8 | data[len+1];
					//Inc len
					len+=2;
				} else {
					//One byte
					pictureId = data[len];
					//Inc len
					len++;
				}
			}
			//check if present
			if (temporalLevelZeroIndexPresent)
			{
				//read it
				temporalLevelZeroIndex = data[len];
				//Inc len
				len++;
			}
			//Check present
			if (temporalLayerIndexPreset || keyIndexPresent)
			{
				//read it
				temporalLevelZeroIndex	= data[len] >> 6;
				layerSync		= data[len] >> 5 & 0x01;
				keyIndex		= data[len] & 0x1F;
				//Inc len
				len++;
			}
		}

		return len;
	}

	DWORD Serialize(BYTE *data,DWORD size)
	{
		//Check size
		if (size<GetSize())
			//Error
			return 0;

		/*
		   The first octets after the RTP header are the VP8 payload descriptor,
		   with the following structure.

		       0 1 2 3 4 5 6 7
		      +-+-+-+-+-+-+-+-+
		      |X|R|N|S|PartID | (REQUIRED)
		      +-+-+-+-+-+-+-+-+
		   X: |I|L|T|K|RSV-A  | (OPTIONAL)
		      +-+-+-+-+-+-+-+-+
		   I: |   PictureID   | (OPTIONAL 8b/16b)
		      +-+-+-+-+-+-+-+-+
		   L: |   TL0PICIDX   | (OPTIONAL)
		      +-+-+-+-+-+-+-+-+
		   T: |TID|Y|  RSV-B  | (OPTIONAL)
		      +-+-+-+-+-+-+-+-+
		 */

		data[0] = extendedControlBitsPresent;
		data[0] = data[0]<<2 | nonReferencePicture;
		data[0] = data[0]<<1 | startOfPartition;
		data[0] = data[0]<<4 | (partitionIndex & 0x0F);

		//If nothing more present
		if (!extendedControlBitsPresent)
			//Exit now
			return 1;
		data[1] = pictureIdPresent;
		data[1] = data[1] << 1 | temporalLevelZeroIndexPresent;
		data[1] = data[1] << 1 | temporalLayerIndexPreset;
		data[1] = data[1] << 1 | keyIndex;
		data[1] = data[1] << 4;

		DWORD len = 2;

		//Check if picture id present
		if (pictureIdPresent)
		{
			//Check long is the picture id
			if (pictureId>>7)
			{
				//Set picture id
				data[len]  = pictureId>>8 | 0x80;
				data[len+1] = pictureId & 0x7F;
				//Inc lenght
				len+=2;
			} else {
				//Set picture id
				data[len] = pictureId & 0x7F;
				//Inc lenght
				len++;
			}
		}

		if (temporalLevelZeroIndexPresent)
		{
			//Set picture id
			data[len] = temporalLevelZeroIndexPresent;
			//Inc lenght
			len++;
		}

		if (temporalLayerIndex || keyIndexPresent)
		{
			//Set picture id
			data[len] = temporalLayerIndex;
			data[len] = data[len]<<2 | layerSync;
			data[len] = data[len]<<1 | keyIndex & 0x1F;
			//Inc lenght
			len++;
		}

		return len;
	}

	void Dump()
	{
		Debug("[VP8PayloadDescriptor \n");
		Debug("\t extendedControlBitsPresent=%d\n"	, extendedControlBitsPresent);
		Debug("\t nonReferencePicture=%d\n"		, nonReferencePicture);
		Debug("\t startOfPartition=%d\n"		, startOfPartition);
		Debug("\t partitionIndex=%d\n"			, partitionIndex);
		Debug("\t pictureIdPresent=%d\n"		, pictureIdPresent);
		Debug("\t temporalLevelZeroIndexPresent=%d\n"	, temporalLevelZeroIndexPresent);
		Debug("\t temporalLayerIndexPreset=%d\n"	, temporalLayerIndexPreset);
		Debug("\t keyIndexPresent=%d\n"			, keyIndexPresent);
		Debug("\t layerSync=%d\n"			, layerSync);
		Debug("\t pictureId=%u\n"			, pictureId);
		Debug("\t temporalLevelZeroIndex=%d\n"		, temporalLevelZeroIndex);
		Debug("\t temporalLayerIndex=%d\n"		, temporalLayerIndex);
		Debug("\t keyIndex=%d\n"			, keyIndex);
		Debug("/]\n");
	}
};

struct VP8PayloadHeader
{
	DWORD size;
	bool showFrame;
	bool isPFrame;
	BYTE version;

	VP8PayloadHeader(BYTE profile, DWORD size,bool isPFrame)
	{
		this->size = size;
		this->isPFrame = isPFrame;
		showFrame = true;
		version = profile;
	}

	DWORD GetSize() {
		return 3;
	}
};
#endif	/* VP8_H */

