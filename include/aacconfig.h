/* 
 * File:   aac.h
 * Author: Sergio
 *
 * Created on 24 de junio de 2013, 12:45
 */

#ifndef AACCONFIG_H
#define	AACCONFIG_H

#include "config.h"
#include "bitstream.h"

class AACSpecificConfig
{
public:
	AACSpecificConfig(DWORD rate,BYTE channels)
	{
		//Put bytes
		BitWritter writter(data,24);
		
		//object type - AAC-LC
		writter.Put(5,2);
		//Get rate index
		BYTE index = GetSampleRateIndex(rate);
		//Set index rate
		writter.Put(4, index);
		//If not found
		if (index==0x0F)
			//Set rate
			writter.Put(24,rate);
		//Set cannles
		writter.Put(4, channels);
		//GASpecificConfig
		writter.Put(1, 0); //frame length - 1024 samples
		writter.Put(1, 0); //does not depend on core coder
		writter.Put(1, 0); //is not extension

		//Flush
		size = writter.Flush();
	}

	BYTE GetSize() const
	{
		return size;
	}

	const BYTE* GetData() const
	{
		return data;
	}
	
	BYTE GetSampleRateIndex(DWORD rate)
	{
		const int rates[16] = {
			    96000, 88200, 64000, 48000, 44100, 32000,
			    24000, 22050, 16000, 12000, 11025, 8000, 7350
			};
		//Search
		for (int i=0;i<sizeof(rates);i++)
			//Check rate
			if (rates[i]==rate)
				//Found
				return i;
		//Not found
		return 0xF;

	}

private:
	BYTE data[24];
	DWORD size;
};



#endif	/* AAC_H */

