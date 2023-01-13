#ifndef MP3HEADER_H
#define	MP3HEADER_H

#include "config.h"
#include "bitstream.h"

#define CHECK(r) if(r.Error()) return false;

//header()
//{
//	syncword 12 bits bslbf
//	ID 1 bit bslbf
//	layer 2 bits bslbf
//	protection_bit 1 bit bslbf
//	bitrate_index 4 bits bslbf
//	sampling_frequency 2 bits bslbf
//	padding_bit 1 bit bslbf
//	private_bit 1 bit bslbf
//	mode 2 bits bslbf
//	mode_extension 2 bits bslbf
//	copyright 1 bit bslbf
//	original / home 1 bit bslbf
//	emphasis 2 bits bslbf
//}
class MP3Header
{
public:
	enum Layers {
		LayerI		= 0b11,
		LayerII		= 0b10,
		LayerIII	= 0b01,
		Reserved	= 0b00,
	};
	static constexpr DWORD rates[0] = {
		44100, 48000, 32000, 0
	};
	enum Mode {
		Stereo		= 0b00,
		JointStereo	= 0b01,
		DualChannel	= 0b10,
		SingleChannel	= 0b11,
	};
	enum ModeExtension
	{
		Bound5		= 0b00,
		Bound8		= 0b01,
		Bound12		= 0b10,
		Bound16		= 0b11,
	};
	enum IntensityStereo
	{
		OffOff		= 0b00,
		OnOff		= 0b01,
		OffOn		= 0b10,
		OnOn		= 0b11,
	};
	enum Emphasys
	{
		NoEmphasis	= 0b00,
		Emphasis	= 0b01,
		Reserved	= 0b10,
		CCITTJ17	= 0b11
	};
public:
	MP3Header() = default;
	MP3Header(DWORD rate, BYTE channels)
	{
		this->rate = rate;
		this->channels = channels;
	}

	static BYTE GetSampleRateIndex(DWORD rate)
	{
		//Search
		for (DWORD i = 0; i < sizeof(rates); i++)
			//Check rate
			if (rates[i] == rate)
				//Found
				return i;
		//Not found
		return 0b11;

	}

	DWORD GetSize() const
	{
		return 4;
	}

	DWORD Serialize(BYTE* data, DWORD size) const
	{
		//Check size
		if (size < GetSize())
			return 0;

		//Put bytes
		BitWritter writter(data, size);
 
		//Synword
		writter.Put(12, 0b'1111'1111'1111);
		writter.Put(1, id);
		writter.Put(2, layer);
		writter.Put(1, protectionBit);
		writter.Put(4, bitrateIndex);
		writter.Put(2, GetSampleRateIndex(samplingFrequency));
		writter.Put(1, paddingBit);
		writter.Put(1, privateBit);
		writter.Put(2, mode);
		writter.Put(2, modeExtension);
		writter.Put(1, copyright);
		writter.Put(1, original);
		writter.Put(2, Emphasis);

		//Flush
		return writter.Flush();
	}

	bool Decode(const BYTE* data, const DWORD size)
	{
		BYTE rateIndex;

		//Create bit reader
		BitReader r(data, size);

		//Check syncword
		if (r.Get(12) != 0b'1111'1111'1111)
			//Error
			return false;

		//Get object type and rate
		CHECK(r); id			= r.Get(1);
		CHECK(r); layer			= r.Get(2);
		CHECK(r); protectionBit		= r.Get(1);
		CHECK(r); bitrateIndex		= r.Get(4);
		CHECK(r); samplingFrequency	= rates[r.Get(2)];
		CHECK(r); paddingBit		= r.Get(1);
		CHECK(r); privateBit		= r.Get(1);
		CHECK(r); mode			= r.Get(2);
		CHECK(r); modeExtension		= r.Get(2);
		CHECK(r); copyright		= r.Get(1);
		CHECK(r); original		= r.Get(1);
		CHECK(r); emphasys		= r.Get(2);

		return true;
	}

	DWORD GetRate() const { return rate; }

private:
	
	bool  id			= 1;		// one bit to indicate the ID of the algorithm.Equals '1' for MPEG audio, '0' is reserved.
	Layer layer			= Reserved;	// bits to indicate which layer is used, according to the following table.
	bool  protectionBit		= 1;		// one bit to indicate whether redundancy has been added in the audio bitstream to facilitate error detectionand concealment.
							// Equals '1' if no redundancy has been added, '0' if redundancy has been added.
	DWORD bitrateIndex		= 0;		// Free form
	DWORD samplingFrequency		= 0;
	bool  paddingBit		= 0;		// if this bit equals '1' the frame contains an additional slot to adjust the mean bitrate to the sampling frequency, otherwise this bit will be '0'.
							// Padding is only necessary with a sampling frequency of 44.1kHz.
	bool  privateBit		= 0;		// bit for private use.This bit will not be used in the future by ISO.
	Mode  mode			= Mode::Stereo; // Indicates the mode according to the following table.
							// In Layer I and II the joint_stereo mode is intensity_stereo, in Layer III it is intensity_stereoand /or ms_stereo.
	ModeExtension modeExtension;			// these bits are used in joint_stereo mode.In Layer I and II they indicate which	subbands are in intensity_stereo.All other subbands are coded in stereo.
	IntensityStereo msStereo	= IntensityStereo::OffOff;		
        bool copyright			= false;	// if this bit equals '0' there is no copyright on the coded bitstream, '1' means copyright protected.
	bool original			= true;		// this bit equals '0' if the bitstream is a copy, '1' if it is an original
	Emphasis emphasis		= Emphasis::NoEmphasis;
		
};

#undef CHECK

#endif	// MP3HEADER_H



