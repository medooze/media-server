#ifndef OPUSCONFIG_H
#define OPUSCONFIG_H
#include "config.h"
#include "tools.h"

/**
5.1.  Identification Header

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |      'O'      |      'p'      |      'u'      |      's'      |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |      'H'      |      'e'      |      'a'      |      'd'      |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  Version = 1  | Channel Count |           Pre-skip            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                     Input Sample Rate (Hz)                    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |   Output Gain (Q7.8 in dB)    | Mapping Family|               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               :
     |                                                               |
     :               Optional Channel Mapping Table...               :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 
 * For mp4 it is sligthy different
    https://vfrmaniac.fushizen.eu/contents/opus_in_isobmff.html
             class ChannelMappingTable (unsigned int(8) OutputChannelCount) {
                unsigned int(8) StreamCount;
                unsigned int(8) CoupledCount;
                unsigned int(8 * OutputChannelCount) ChannelMapping;
            }

            aligned(8) class OpusDecoderConfigurationRecord {
                unsigned int(8) Version;
                unsigned int(8) OutputChannelCount;
                unsigned int(16) PreSkip;
                unsigned int(32) InputSampleRate;
                signed int(16) OutputGain;
                unsigned int(8) ChannelMappingFamily;
                if (ChannelMappingFamily != 0) {
                    ChannelMappingTable(OutputChannelCount);
                }
            }
	    + Version:
                The Version field shall be set to 0.
                In the future versions of this specification, this field may be set to other values. And without support
                of those values, the reader shall not read the fields after this within the OpusSpecificBox.
            + OutputChannelCount:
                The OutputChannelCount field shall be set to the same value as the *Output Channel Count* field in the
                identification header defined in Ogg Opus [3].
            + PreSkip:
                The PreSkip field indicates the number of the priming samples, that is, the number of samples at 48000 Hz
                to discard from the decoder output when starting playback. The value of the PreSkip field shall be at least
                80 milliseconds' worth of PCM samples even when removing any number of Opus samples which may or may not
                contain the priming samples. The PreSkip field is not used for discarding the priming samples at the whole
                playback at all since it is informative only, and that task falls on the Edit List Box.
            + InputSampleRate:
                The InputSampleRate field shall be set to the same value as the *Input Sample Rate* field in the
                identification header defined in Ogg Opus [3].
            + OutputGain:
                The OutputGain field shall be set to the same value as the *Output Gain* field in the identification
                header define in Ogg Opus [3]. Note that the value is stored as 8.8 fixed-point.
            + ChannelMappingFamily:
                The ChannelMappingFamily field shall be set to the same value as the *Channel Mapping Family* field in
                the identification header defined in Ogg Opus [3]. Note that the value 255 may be used for an alternative
                to map channels by ISO Base Media native mapping. The details are described in 4.5.1.
            + StreamCount:
                The StreamCount field shall be set to the same value as the *Stream Count* field in the identification
                header defined in Ogg Opus [3].
            + CoupledCount:
                The CoupledCount field shall be set to the same value as the *Coupled Count* field in the identification
                header defined in Ogg Opus [3].
            + ChannelMapping:
                The ChannelMapping field shall be set to the same octet string as *Channel Mapping* field in the identi-
                fication header defined in Ogg Opus [3].
 */

class OpusConfig
{
public:
	OpusConfig() = default;
	OpusConfig(uint8_t  outputChannelCount, uint32_t inputSampleRate) :
		outputChannelCount(outputChannelCount),
		inputSampleRate(inputSampleRate)
	{}
	
	size_t GetSize() const { return 19; }
	
	size_t Serialize(uint8_t* data, size_t size) const
	{
		const char* MagicSignarure = "OpusHead";
		
		//Check size
		if (size<GetSize())
			return 0;
		
		//Copy magic signature
		memcpy(data,MagicSignarure,8);
		
		//Write data
		set1(data, 8, version);
		set1(data, 9, outputChannelCount);
		set2(data, 10, preSkip);
		set4(data, 12, inputSampleRate);
		set2(data, 16, outputGain);
		set1(data, 18, channelMappingFamily);
		
		//Done
		return 19;
	}
	
	void SetOutputChannelCount(uint8_t outputChannelCount)
	{
		this->outputChannelCount = outputChannelCount;
	}
	
	uint8_t GetOutputChannelCount() const { return outputChannelCount; }
private:
	uint8_t  version		= 1;
	uint8_t  outputChannelCount	= 1;
	uint16_t preSkip		= 0;
	uint32_t inputSampleRate	= 48000;
	int16_t  outputGain		= 0;
	uint8_t  channelMappingFamily	= 0;
};




class OpusTOC
{
public:
	enum Mode
	{
		SILKOnly,
		Hybrid,
		CELTOnly
	};
	
	enum CodeNumber
	{
		One = 0,
		Two = 1,
		Three = 2,
		Arbitrary = 3,
	};
	
	enum Bandwitdh
	{
		Narrowband,
		MediumBand,
		WideBand,
		SuperWideBand,
		FullBand,
	};
	
	enum FrameSize 
	{
		ms2_5	= 120,
		ms5	= 240,
		ms10    = 480,
		ms20	= 960,
		ms40	= 1920,
		ms60	= 2880
	};
	
	/**
	* 
	       +-----------------------+-----------+-----------+-------------------+
	       | Configuration         | Mode      | Bandwidth | Frame Sizes       |
	       | Number(s)             |           |           |                   |
	       +-----------------------+-----------+-----------+-------------------+
	       | 0...3                 | SILK-only | NB        | 10, 20, 40, 60 ms |
	       |                       |           |           |                   |
	       | 4...7                 | SILK-only | MB        | 10, 20, 40, 60 ms |
	       |                       |           |           |                   |
	       | 8...11                | SILK-only | WB        | 10, 20, 40, 60 ms |
	       |                       |           |           |                   |
	       | 12...13               | Hybrid    | SWB       | 10, 20 ms         |
	       |                       |           |           |                   |
	       | 14...15               | Hybrid    | FB        | 10, 20 ms         |
	       |                       |           |           |                   |
	       | 16...19               | CELT-only | NB        | 2.5, 5, 10, 20 ms |
	       |                       |           |           |                   |
	       | 20...23               | CELT-only | WB        | 2.5, 5, 10, 20 ms |
	       |                       |           |           |                   |
	       | 24...27               | CELT-only | SWB       | 2.5, 5, 10, 20 ms |
	       |                       |           |           |                   |
	       | 28...31               | CELT-only | FB        | 2.5, 5, 10, 20 ms |
	       +-----------------------+-----------+-----------+-------------------+
	*/
private:
	static inline std::vector<std::tuple<Mode,Bandwitdh,FrameSize>> Configurations = 
	{
		{SILKOnly	, Narrowband	, ms10},
		{SILKOnly	, Narrowband	, ms20},
		{SILKOnly	, Narrowband	, ms40},
		{SILKOnly	, Narrowband	, ms60},
		{SILKOnly	, MediumBand	, ms10},
		{SILKOnly	, MediumBand	, ms20},
		{SILKOnly	, MediumBand	, ms40},
		{SILKOnly	, MediumBand	, ms60},
		{SILKOnly	, WideBand	, ms10},
		{SILKOnly	, WideBand	, ms20},
		{SILKOnly	, WideBand	, ms40},
		{SILKOnly	, WideBand	, ms60},
		{Hybrid		, SuperWideBand	, ms10},
		{Hybrid		, SuperWideBand	, ms20},
		{Hybrid		, FullBand	, ms10},
		{Hybrid		, FullBand	, ms20},
		{CELTOnly	, Narrowband	, ms2_5},
		{CELTOnly	, Narrowband	, ms5},
		{CELTOnly	, Narrowband	, ms10},
		{CELTOnly	, Narrowband	, ms20},
		{CELTOnly	, WideBand	, ms2_5},
		{CELTOnly	, WideBand	, ms5},
		{CELTOnly	, WideBand	, ms10},
		{CELTOnly	, WideBand	, ms20},
		{CELTOnly	, SuperWideBand	, ms2_5},
		{CELTOnly	, SuperWideBand	, ms5},
		{CELTOnly	, SuperWideBand	, ms10},
		{CELTOnly	, SuperWideBand	, ms20},
		{CELTOnly	, FullBand	, ms2_5},
		{CELTOnly	, FullBand	, ms5},
		{CELTOnly	, FullBand	, ms10},
		{CELTOnly	, FullBand	, ms20},
		
		
	};
public:
	
	/**
	 *	TOC byte
	 *      0
		0 1 2 3 4 5 6 7
	       +-+-+-+-+-+-+-+-+
	       | config  |s| c |
	       +-+-+-+-+-+-+-+-+

	 */
	static std::tuple<Mode,Bandwitdh,FrameSize,bool,CodeNumber> TOC(uint8_t toc)
	{
		//Get config
		uint8_t config = toc >> 3;
		//Stereo
		bool stereo = toc & 0x04;
		//Code number
		CodeNumber codeNumber = static_cast<CodeNumber>(toc | 0x03);
		//Get mode and ruation
		auto [mode, bandwidth, size ] = Configurations[config]; 
		//Done
		return std::tuple<Mode,Bandwitdh,FrameSize,bool,CodeNumber>{mode, bandwidth, size, stereo, codeNumber};
	}
	
	static uint8_t GetTOC(Mode mode, Bandwitdh bandwidth, FrameSize size, bool stereo, CodeNumber number)
	{
		uint8_t config = 0;
		//Check which configuration is it
		for (size_t i=0; i<Configurations.size(); ++i)
		{
			if (std::get<0>(Configurations[i]) == mode && std::get<1>(Configurations[i]) == bandwidth && std::get<2>(Configurations[i]) == size)
			{
				//This is it
				config = i;
				//Done
				break;
			}
		}
		//Create toc byte
		return config << 3 | (stereo ? 0x04: 0x00) | (number & 0x03);
	}


	static bool IsValid(Mode mode, Bandwitdh bandwidth, FrameSize size)
	{
		//Check which configuration is it
		for (size_t i=0; i<Configurations.size(); ++i)
			if (std::get<0>(Configurations[i]) == mode && std::get<1>(Configurations[i]) == bandwidth && std::get<2>(Configurations[i]) == size)
				return true;
		return false;
	}

};
#endif /* OPUSCONFIG_H */

