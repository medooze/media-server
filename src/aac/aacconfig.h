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

#define CHECK(r) if(r.Error()) return false;

//AudioSpecificConfig ()
//{
//	AudioObjectType 5 bslbf
//	samplingFrequencyIndex 4 bslbf
//	if( samplingFrequencyIndex==0xf )
//		samplingFrequency 24 uimsbf
//	channelConfiguration 4 bslbf
//	if( AudioObjectType == 1 || AudioObjectType == 2 ||
//		AudioObjectType == 3 || AudioObjectType == 4 ||
//		AudioObjectType == 6 || AudioObjectType == 7 )
//		GASpecificConfig()
//}
//
//GASpecificConfig ( samplingFrequencyIndex, channelConfiguration,audioObjectType )
//{
//	FrameLength; 1 bslbf
//	DependsOnCoreCoder; 1 bslbf
//	if ( dependsOnCoreCoder ) {
//		coreCoderDelay; 14 uimsbf
//	}
//	ExtensionFlag 1 bslbf
//	if ( ! ChannelConfiguration ) {
//		program_config_element ();
//	}
//	if ( extensionFlag ) {
//		if ( AudioObjectType==22 ) {
//			numOfSubFrame 5 bslbf
//			layer_length 11 bslbf
//		}
//		If(AudioObjectType==17 || AudioObjectType == 18 ||
//		AudioObjectType == 19 || AudioObjectType == 20 ||
//		AudioObjectType == 21 || AudioObjectType == 23){
//			AacSectionDataResilienceFlag; 1 bslbf
//			AacScalefactorDataResilienceFlag; 1 bslbf
//			AacSpectralDataResilienceFlag; 1 bslbf
//		}
//		extensionFlag3; 1 bslbf
//		if ( extensionFlag3 ) {
//		/* tbd in version 3 */
//		}
//	}
//}
class AACSpecificConfig
{
public:
	static constexpr std::array<DWORD, 13> rates = {
		96000, 88200, 64000, 48000, 44100, 32000,
		24000, 22050, 16000, 12000, 11025,  8000,
		7350
	};
public:
	AACSpecificConfig() = default;
	AACSpecificConfig(DWORD rate,BYTE channels)
	{
		this->rate = rate;
		this->channels = channels;
	}

	static BYTE GetSampleRateIndex(DWORD rate)
	{
		//Search
		for (DWORD i=0;i<rates.size();i++)
			//Check rate
			if (rates[i]==rate)
				//Found
				return i;
		//Not found
		return 0x0F;

	}
	
	DWORD GetSize() const
	{
		return GetSampleRateIndex(rate) != 0xf ? 2 : 5;
	}
	
	DWORD Serialize(BYTE* data,DWORD size) const
	{
		//Check size
		if (size<GetSize())
			return 0;
		
		//Put bytes
		BitWritter writter(data,size);
		
		//object type - AAC-LC
		writter.Put(5,objectType);
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
		writter.Put(1, frameLength); 
		writter.Put(1, coreCoder); 
		writter.Put(1, extension); 
		
		//Flush
		return writter.Flush();
	}
	
	bool Decode(const BYTE* data,const DWORD size)
	{
		BYTE rateIndex;
		
		//Create bit reader
		BitReader r(data,size);
		
		//Get object type and rate
		CHECK(r); objectType = r.Get(5);
		CHECK(r); rateIndex = r.Get(4);
		//Check rate index
		if (rateIndex<rates.size())
		{
			//Get rate from table
			rate = rates[rateIndex];
		} else if (rateIndex==0x0F) {
			//Get raw rate
			CHECK(r); rate = r.Get(24);
		} else {
			//Wrong rate
			return 0;
		}
		//Get channel config
		CHECK(r);channels = r.Get(4);
		//Get GASpecific config
		if( objectType == 1 || objectType == 2 || objectType == 3 || objectType == 4 || objectType == 6 || objectType == 7 )
		{
			CHECK(r); frameLength = r.Get(1);
			CHECK(r); coreCoder = r.Get(1);
			if (coreCoder)
			{
				CHECK(r); coreCoderDelay = r.Get(14);
			}
			CHECK(r); extension = r.Get(1);
			
			//TODO: handle extensions
		} else {
			//TODO: not supported
		}
		return true;
	}

	void Dump() const
	{
		Debug("[AACSpecificConfig \n");
		Debug("\t objectType=%u\n"	, objectType);
		Debug("\t rate=%u\n"		, rate);
		Debug("\t channels=%u\n"	, channels);
		Debug("\t frameLength=%u\n"	, frameLength);
		Debug("\t coreCoder=%u\n"	, coreCoder);
		Debug("\t coreCoderDelay=%u\n"	, coreCoderDelay);
		Debug("\t extension=%u\n"	, extension);
		Debug("/]\n");
	}
	
	BYTE  GetObjectType() const	{ return objectType;			}
	DWORD GetRate() const		{ return rate;				}
	BYTE  GetChannels() const	{ return channels;			}
	DWORD GetFrameLength() const	{ return frameLength ? 960 : 1024;	}
	
private:
	BYTE objectType		 = 2;		//AAC-LC
	DWORD rate		 = 0;
	BYTE channels		 = 0;
	bool frameLength	 = false;	//frame length - 1024 samples
	bool coreCoder		 = false;	//does not depend on core coder
	DWORD coreCoderDelay	 = 0;		//Unused
	bool extension		 = false;	//is not extension
};

#undef CHECK

#endif	/* AAC_H */

