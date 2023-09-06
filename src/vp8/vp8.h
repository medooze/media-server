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
	BYTE pictureIdLength;
	bool temporalLevelZeroIndexPresent;
	bool temporalLayerIndexPresent;
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
		pictureIdLength = 1;
		temporalLevelZeroIndexPresent = 0;
		temporalLayerIndexPresent = 0;
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
		pictureIdPresent = 0;
		pictureIdLength = 1;
		temporalLevelZeroIndexPresent = 0;
		temporalLayerIndexPresent = 0;
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

	DWORD GetSize() const
	{
		//Base size
		DWORD len = 1;
		if (extendedControlBitsPresent)
		{
			len++;
			if (pictureIdPresent)
				len += pictureIdLength;
			if (temporalLevelZeroIndexPresent)
				len++;
			if (temporalLayerIndexPresent || keyIndexPresent)
				len++;
		}
		return len;
	}

	DWORD Parse(const BYTE* data, DWORD size)
	{
		//Check size
		if (size<1)
			//Invalid
			return 0;
		//Start parsing
		DWORD len = 1;

		//Read first
		extendedControlBitsPresent	= data[0] >> 7;
		nonReferencePicture		= data[0] >> 5 & 0x01;
		startOfPartition		= data[0] >> 4 & 0x01;
		partitionIndex			= data[0] & 0x0F;

		//check if more
		if (extendedControlBitsPresent)
		{
			//Check size
			if (size<len+1)
				//Invalid
				return 0;

			//Read second
			pictureIdPresent		= data[1] >> 7;
			temporalLevelZeroIndexPresent	= data[1] >> 6 & 0x01;
			temporalLayerIndexPresent 	= data[1] >> 5 & 0x01;
			keyIndexPresent			= data[1] >> 4 & 0x01;
			//Increase len
			len++;

			//Check if we need to read picture id
			if (pictureIdPresent)
			{
				//Check size
				if (size<len+1)
					//Invalid
					return 0;
				//Check mark
				if (data[len] & 0x80)
				{
					//Check size again
					if (size<len+2)
						//Invalid
						return 0;
					//Two bytes
					pictureIdLength = 2;
					pictureId = (data[len] & 0x7F) << 8 | data[len+1];
					//Inc len
					len+=2;
				} else {
					//One byte
					pictureIdLength = 1;
					pictureId = data[len];
					//Inc len
					len++;
				}
			}
			//check if present
			if (temporalLevelZeroIndexPresent)
			{
				//Check size
				if (size<len+1)
					//Invalid
					return 0;
				//read it
				temporalLevelZeroIndex = data[len];
				//Inc len
				len++;
			}
			//Check present
			if (temporalLayerIndexPresent || keyIndexPresent)
			{
				//Check size
				if (size<len+1)
					//Invalid
					return 0;
				//read it
				temporalLayerIndex	= data[len] >> 6;
				layerSync		= data[len] >> 5 & 0x01;
				keyIndex		= data[len] & 0x1F;
				//Inc len
				len++;
			}
		}

		return len;
	}

	DWORD Serialize(BYTE *data,DWORD size) const
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
		data[1] = data[1] << 1 | temporalLayerIndexPresent;
		data[1] = data[1] << 1 | keyIndexPresent;
		data[1] = data[1] << 4;

		DWORD len = 2;

		//Check if picture id present
		if (pictureIdPresent)
		{
			//Check how long is the picture id
			if (pictureIdLength == 2)
			{
				//Set picture id
				data[len]  = pictureId>>8 | 0x80;
				data[len+1] = pictureId & 0xFF;
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
			data[len] = temporalLevelZeroIndex;
			//Inc lenght
			len++;
		}

		if (temporalLayerIndexPresent || keyIndexPresent)
		{
			//Set picture id
			data[len] = temporalLayerIndex;
			data[len] = data[len]<<1 | layerSync;
			data[len] = data[len]<<1 | (keyIndex & 0x1F);
			data[len] = data[len]<<4;
			//Inc lenght
			len++;
		}

		return len;
	}

	void Dump() const
	{
		Debug("[VP8PayloadDescriptor \n");
		Debug("\t extendedControlBitsPresent=%d\n"	, extendedControlBitsPresent);
		Debug("\t nonReferencePicture=%d\n"		, nonReferencePicture);
		Debug("\t startOfPartition=%d\n"		, startOfPartition);
		Debug("\t partitionIndex=%d\n"			, partitionIndex);
		Debug("\t pictureIdPresent=%d\n"		, pictureIdPresent);
		Debug("\t temporalLevelZeroIndexPresent=%d\n"	, temporalLevelZeroIndexPresent);
		Debug("\t temporalLayerIndexPreset=%d\n"	, temporalLayerIndexPresent);
		Debug("\t keyIndexPresent=%d\n"			, keyIndexPresent);
		Debug("\t layerSync=%d\n"			, layerSync);
		Debug("\t pictureIdLength=%d\n"			, pictureIdLength);
		Debug("\t pictureId=%u\n"			, pictureId);
		Debug("\t temporalLevelZeroIndex=%d\n"		, temporalLevelZeroIndex);
		Debug("\t temporalLayerIndex=%d\n"		, temporalLayerIndex);
		Debug("\t keyIndex=%d\n"			, keyIndex);
		Debug("/]\n");
	}
};

struct VP8PayloadHeader
{
	bool showFrame = 0;
	bool isKeyFrame = 0;
	BYTE version = 0;
	DWORD firstPartitionSize = 0;
	WORD width = 0;
	WORD height = 0;
	BYTE horizontalScale = 0;
	BYTE verticalScale = 0;

	DWORD Parse(const BYTE* data, DWORD size)
	{
		//Check size
		if (size<3)
			//Invalid
			return 0;

		//Read comon 3 bytes
		//   0 1 2 3 4 5 6 7
                //  +-+-+-+-+-+-+-+-+
                //  |Size0|H| VER |P|
                //  +-+-+-+-+-+-+-+-+
                //  |     Size1     |
                //  +-+-+-+-+-+-+-+-+
                //  |     Size2     |
                //  +-+-+-+-+-+-+-+-+
		firstPartitionSize	= get3Reversed(data, 0) >> 5;
		showFrame		= data[0] >> 4 & 0x01;
		version			= data[0] >> 1 & 0x07;
		isKeyFrame		= (data[0] & 0x01) == 0;

		//check if more
		if (isKeyFrame)
		{
			//Check size
			if (size<10)
				//Invalid
				return Error("Invalid intra size [%d]\n",size);
			//Check Start code
			if (get3(data,3)!=0x9d012a)
			{
				::Dump4(data,10);
				//Invalid
				return Error("Invalid start code [%u]\n",get3(data,3));
			}
			//Get size in le
			WORD hor = data[7]<<8 | data[6];
			WORD ver = data[9]<<8 | data[8];
			//Get dimensions and scale
			width		= hor & 0x3fff;
			horizontalScale = hor >> 14;
			height		= ver & 0x3fff;
			verticalScale	= ver >> 14;
			//Key frame
			return 10;
		}
		//No key frame
		return 3;
	}

	DWORD GetSize() const
	{
		return isKeyFrame ? 10 : 3;
	}

	void Dump() const
	{
		Debug("[VP8PayloadHeader \n");
		Debug("\t isKeyFrame=%d\n"		, isKeyFrame);
		Debug("\t version=%d\n"			, version);
		Debug("\t showFrame=%d\n"		, showFrame);
		Debug("\t firtPartitionSize=%d\n"	, firstPartitionSize);
		if (isKeyFrame)
		{
			Debug("\t width=%d\n"		, width);
			Debug("\t horizontalScale=%d\n"	, horizontalScale);
			Debug("\t height=%d\n"		, height);
			Debug("\t verticalScale=%d\n"	, verticalScale);
		}
		Debug("/]\n");
	}
};

class VP8CodecConfig
{
public:
	DWORD Serialize(BYTE* buffer,DWORD length) const
	{
		if (length<GetSize())
			return 0;

		int l = 0;

		buffer[l++] = profile;
		buffer[l++] = level;
		buffer[l++] = (bitDepth << 4) | (chromaSubsampling << 1) | (videoFullRangeFlag ? 1 : 0);
		buffer[l++] = colorPrimaries;
		buffer[l++] = transferCharacteristics;
		buffer[l++] = matrixCoefficients;
		//add initialization data length
		set2(buffer,l,initializationData.size());
		//Inc leght
		l += 2;
		//Copy it
		memcpy(buffer+l,initializationData.data(),initializationData.size());
		//Inc length
		l += initializationData.size();
		//Done
		return l;
	}

	DWORD GetSize() const
	{
		return 8 + initializationData.size();
	}
private:
	uint8_t profile			= 0;
	uint8_t level			= 10;
	uint8_t bitDepth		= 8;
	uint8_t chromaSubsampling	= 0; //CHROMA_420
	bool videoFullRangeFlag		= false;
	uint8_t colorPrimaries		= 2; //Unspecified
	uint8_t transferCharacteristics	= 2; //Unspecified
	uint8_t matrixCoefficients	= 2; //Unspecified
	std::vector<uint8_t> initializationData;
};
#endif	/* VP8_H */
