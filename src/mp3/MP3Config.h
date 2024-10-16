#ifndef MP3CONFIG_H
#define	MP3CONFIG_H

#include "config.h"
#include "bitstream/BitReader.h"
#include "bitstream/BitWriter.h"

class MP3Config
{
public:
	enum class MPEGAudioVersion {
		MPEGVersion2,
		MPEGVersion1,
		MPEGVersionReserved
	};
	enum class MPEGAudioLayer {
		MPEGLayer3,
		MPEGLayerReserved
	};

	static constexpr std::array<DWORD, 3> Mpeg1SamplingRates = {44100, 48000, 32000};
	static constexpr std::array<DWORD, 15> Mpeg1Bitrates = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};

	static constexpr std::array<DWORD, 3> Mpeg2SamplingRates = {22050, 24000, 16000};
	static constexpr std::array<DWORD, 15> Mpeg2Bitrates = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
public:
	
	MP3Config() = default;
	MP3Config(DWORD rate, BYTE channels)
	{
		this->samplingRate = rate;
		this->channels = channels;
	}

	bool Decode(const BYTE* data,const DWORD size)
	{
		if (size != 4) 
			return Error("-MP3Config::Decode() config data must be size of 4 bytes\n");
		
		//Create bit reader
		BufferReader reader(data,size);
		BitReader r(reader);

		try{
			auto frameSync = r.Get(11);
            if (frameSync != 0x7FF) 
                return Error("-MP3Config::Decode() Wrong FrameSync\n");
		
			auto ver  = r.Get(2);
            switch (ver) {
                case 2:
                    audioVersion = MPEGAudioVersion::MPEGVersion2;
                    break;
                case 3:
                    audioVersion = MPEGAudioVersion::MPEGVersion1;
                    break;
                default:
                    return Error("-MP3Config::Decode() MPEG audio version not supported\n");
            }; 
			auto layer  = r.Get(2);
			if (layer == 1)
			{
				audioLayer = MPEGAudioLayer::MPEGLayer3;
				frameLength = audioVersion == MPEGAudioVersion::MPEGVersion1 ? 1152 : 576;
			}	
			else
			{
				return Error("-MP3Config::Decode() MPEG audio layer not supported\n");
			}
			r.Skip(1);

			auto bitrateIdx = r.Get(4);
			if (bitrateIdx >= 15) 
				return Error("-MP3Config::Decode() bitrate idx out of range\n");
			else if (bitrateIdx == 0)
				return Error("-MP3Config::Decode() free bitrate idx not supported\n");
			
			bitrate = audioVersion == MPEGAudioVersion::MPEGVersion1 ? Mpeg1Bitrates[bitrateIdx] : Mpeg2Bitrates[bitrateIdx];

			auto samplingRateIdx = r.Get(2);
			if (samplingRateIdx >= 3) 
				return Error("-MP3Config::Decode() invalid sampling rate idx\n");

            samplingRate = audioVersion == MPEGAudioVersion::MPEGVersion1 ? Mpeg1SamplingRates[samplingRateIdx] : Mpeg2SamplingRates[samplingRateIdx];

			padding = r.Get(1);
			if (padding)
				paddingSize = 1;
			
			r.Skip(1);
			auto channelMode = r.Get(2);
			if (channelMode == 3)
				channels = 1;
			else 
				channels = 2;
		}
		catch (std::exception& e)
		{
			return false;
		}
		return true;
	}

	void Dump() const
	{
		Debug("[MP3Config \n");
		Debug("\t mpegVersion=%u\n"	, audioVersion);
        Debug("\t mpegLayer=%u\n"	, audioLayer);
		Debug("\t rate=%u\n"		, samplingRate);
		Debug("\t channels=%u\n"	, channels);
		Debug("\t paddingSize=%u\n"	, paddingSize);
		Debug("\t frameLength=%u\n"	, frameLength);
		Debug("/]\n");
	}
	MPEGAudioVersion GetAudioVersion() const {return audioVersion;}
	MPEGAudioLayer GetAudioLayer() const {return audioLayer;}
	DWORD GetRate() const		{ return samplingRate;				}
	BYTE  GetChannels() const	{ return channels;			}
	DWORD GetPaddingSize() const	{ return padding ? paddingSize:0; }
	DWORD GetFrameLength() const	{ return frameLength; }
	DWORD GetBitrate() const	{ return bitrate * 1000; }
	
private:
	MPEGAudioVersion audioVersion = MPEGAudioVersion::MPEGVersionReserved;
	MPEGAudioLayer audioLayer = MPEGAudioLayer::MPEGLayerReserved;
	bool padding;
	BYTE paddingSize;
	DWORD samplingRate = 0;
	DWORD bitrate = 0;
	BYTE channels = 0;
	DWORD frameLength = 0;
};
#endif	/* MPEGCONFIG_H */