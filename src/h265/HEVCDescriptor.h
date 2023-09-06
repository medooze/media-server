/* 
 * File:   HEVCDescriptor.h
 * Author: Zita
 *
 * Created on 21 June 2023, 12:21
 */

#ifndef HEVCDESCRIPTOR_H
#define	HEVCDESCRIPTOR_H

#include <vector>
#include "config.h"
#include "h265.h"

class HEVCDescriptor
{
public:
	HEVCDescriptor();
	~HEVCDescriptor();

	void AddVideoParameterSet(const BYTE *data,DWORD size);
	void AddSequenceParameterSet(const BYTE *data,DWORD size);
	void AddPictureParameterSet(const BYTE *data,DWORD size);
	void AddParametersFromFrame(const BYTE *data,DWORD size);

	void ClearVideoParameterSets();
	void ClearSequenceParameterSets();
	void ClearPictureParameterSets();

	DWORD Serialize(BYTE* buffer,DWORD bufferLength) const;
	bool Parse(const BYTE* data,DWORD size);
	void Dump() const;

	BYTE GetNumOfVideoParameterSets()		const { return numOfVideoParameterSets;	}
	BYTE GetNumOfSequenceParameterSets()		const { return numOfSequenceParameterSets;	}
	BYTE GetNumOfPictureParameterSets()		const { return numOfPictureParameterSets;	}
	
	BYTE* GetVideoParameterSet(BYTE i)		const { return vpsData[i];	}
	DWORD GetVideoParameterSetSize(BYTE i)	const { return vpsSizes[i];	}
	BYTE* GetSequenceParameterSet(BYTE i)		const { return spsData[i];	}
	DWORD GetSequenceParameterSetSize(BYTE i)	const { return spsSizes[i];	}
	BYTE* GetPictureParameterSet(BYTE i)		const { return ppsData[i];	}
	DWORD GetPictureParameterSetSize(BYTE i)	const { return ppsSizes[i];	}
	
	BYTE GetConfigurationVersion()		const { return configurationVersion;		}
	BYTE GetNALUnitLengthSizeMinus1()	const { return NALUnitLengthSizeMinus1;		}
	DWORD GetSize()		const { return MediaParameterSize
 								//VPS type(1B) + count(2B) + length(2B) per VPS + total data
								+ 1 + 2 + 2*numOfVideoParameterSets + vpsTotalSizes
								//SPS type(1B) + count(2B) +  length(2B) per SPS + total data 
								+ 1 + 2 + 2*numOfSequenceParameterSets + spsTotalSizes
								//PPS type(1B) + count(2B) + length(2B) per PPS + total data
								+ 1 + 2 + 2*numOfPictureParameterSets + ppsTotalSizes;}
	void SetConfigurationVersion(BYTE in)		{ configurationVersion = in; }

	//profileIndication = (generalProfileSpace << 6) | (generalTierFlag << 5) | (generalProfileIdc & 0x1f);
	void SetGeneralProfileSpace(BYTE in)	{ generalProfileSpace = in; profileIndication &= 0b0011'1111; profileIndication |= (generalProfileSpace << 6); }
	void SetGeneralProfileIdc(BYTE in)	{ generalProfileIdc = in; profileIndication &= 0b1110'0000; profileIndication |= (generalProfileIdc & 0b1'1111); }
	void SetGeneralTierFlag(bool in)	{ generalTierFlag = static_cast<uint8_t> (in); profileIndication &= 0b1101'1111; profileIndication |= (generalTierFlag << 5); }

	void SetGeneralLevelIdc(BYTE in)	{ generalLevelIdc = in; }
	void SetGeneralProfileCompatibilityFlags(const H265ProfileCompatibilityFlags& profile_compatibility_flag)
	{
		generalProfileCompatibilityFlags = 0;
		//static_assert(profile_compatibility_flag.size() == sizeof(generalProfileCompatibilityFlags) * 8);
		for (size_t i = 0; i < profile_compatibility_flag.size(); i++)
		{
			generalProfileCompatibilityFlags |= (profile_compatibility_flag[i] << i);
		}
	}
	void SetGeneralConstraintIndicatorFlags(QWORD in)
	{
		static_assert(constraintIndicatorFlagsSize <= sizeof(in) * 8);
		for (size_t i = 0; i < generalConstraintIndicatorFlags.size(); ++i)
		{
			const BYTE bits_offset = i * 8;
			generalConstraintIndicatorFlags[i] = (in & (0xffULL << bits_offset)) >> bits_offset; 
		}
	}
	void SetNALUnitLengthSizeMinus1(BYTE in)			{ NALUnitLengthSizeMinus1 = in; }

private:
	/* 7.1.  Media Type Registration in RFC 7798*/
	/* ISO/IEC 14496-15 */
	BYTE configurationVersion = 0;
	BYTE generalProfileSpace = 0; // serialized in profileIndication 
	BYTE generalTierFlag = 0; // serialized in profileIndication 
	BYTE generalProfileIdc = 0; // serialized in profileIndication 
	BYTE profileIndication = 0; // (generalProfileSpace << 6) | (generalTierFlag << 5) | (generalProfileIdc & 0x1f);
	DWORD generalProfileCompatibilityFlags = 0;
	static const BYTE constraintIndicatorFlagsSize = 6;
	std::array<BYTE, constraintIndicatorFlagsSize> generalConstraintIndicatorFlags = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};;
	BYTE generalLevelIdc = 0;
	static constexpr std::array<BYTE, 8> paddingBytes = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	// bit(4) reserved = ‘1111’b;
	// unsigned int(12) min_spatial_segmentation_idc;
	// bit(6) reserved = ‘111111’b;
	// unsigned int(2) parallelismType;
	// bit(6) reserved = ‘111111’b;
	// unsigned int(2) chroma_format_idc;
	// bit(5) reserved = ‘11111’b;
	// unsigned int(3) bit_depth_luma_minus8;
	// bit(5) reserved = ‘11111’b;
	// unsigned int(3) bit_depth_chroma_minus8;
	// bit(16) avgFrameRate;
	// bit(2) constantFrameRate;
	// bit(3) numTemporalLayers;
	// bit(1) temporalIdNested;
	// unsigned int(2) lengthSizeMinusOne;
	BYTE NALUnitLengthSizeMinus1 = 0;
	const static BYTE numOfArrays = 3; // let's seperate it for VPS, PPS, SPS right now

	// VPS
    WORD numOfVideoParameterSets = 0; // MAX_VPS_COUNT: 16 bcoz 7.4.2.1: vps_video_parameter_set_id is u(4), but need to be 2bytes to fit the record descriptor
	std::vector<WORD> vpsSizes;
	std::vector<BYTE*> vpsData;
	DWORD vpsTotalSizes = 0;
	// SPS
    WORD numOfSequenceParameterSets = 0; // MAX_SPS_COUNT: 16 bcoz 7.4.3.2.1: sps_seq_parameter_set_id is in [0, 15], but need to be 2bytes to fit the record descriptor
	std::vector<WORD> spsSizes;
	std::vector<BYTE*> spsData;
	DWORD spsTotalSizes = 0;
	// PPS
   	WORD numOfPictureParameterSets = 0; // MAX_PPS_COUNT: 64 bcoz // 7.4.3.3.1: pps_pic_parameter_set_id is in [0, 63], but need to be 2bytes to fit the record descriptor
	std::vector<WORD> ppsSizes;
	std::vector<BYTE*> ppsData;
	DWORD ppsTotalSizes = 0;
public: 
	// serializable header size (in Byte) except VPS/PPS/SPS
	static const BYTE MediaParameterSize = sizeof(configurationVersion)
										+ sizeof(profileIndication)
										+ sizeof(generalProfileCompatibilityFlags)
										+ constraintIndicatorFlagsSize
										+ sizeof(generalLevelIdc)
										+ paddingBytes.size() 
										+ sizeof(NALUnitLengthSizeMinus1)
										+ sizeof(numOfArrays);
};

#endif	/* HEVCDESCRIPTOR_H */

