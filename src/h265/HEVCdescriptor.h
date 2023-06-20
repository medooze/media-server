/* 
 * File:   HEVCDescriptor.h
 * Author: Sergio
 *
 * Created on 21 de junio de 2011, 12:21
 */

#ifndef HEVCDESCRIPTOR_H
#define	HEVCDESCRIPTOR_H

#include "config.h"
#include <vector>

class HEVCDescriptor
{
public:
	HEVCDescriptor();
	~HEVCDescriptor();

	static const BYTE MediaParameterSize = 7; // serializable header size (in Byte) except VPS/PPS/SPS

	void AddVideoParameterSet(const BYTE *data,DWORD size);
	void AddSequenceParameterSet(const BYTE *data,DWORD size, BYTE nuh_layer_id);
	void AddPictureParameterSet(const BYTE *data,DWORD size);
	void AddParametersFromFrame(const BYTE *data,DWORD size, BYTE nuh_layer_id);

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
	BYTE GetNALUnitLength()			const { return NALUnitLength;			}
	DWORD GetSize()		const { return MediaParameterSize 
								+ 2*numOfVideoParameterSets + vpsTotalSizes //VPS: length(2B) + data
								+ (2+1)*numOfSequenceParameterSets + spsTotalSizes //SPS: nul_layer_id(1B) + length(2B) + data
								+ 2*numOfPictureParameterSets + ppsTotalSizes;} //PPS: length(2B) + data
	void SetConfigurationVersion(BYTE configurationVersion)		{ this->configurationVersion = configurationVersion;	}
	void SetNALUnitLength(BYTE NALUnitLength)			{ this->NALUnitLength = NALUnitLength;			}

private:
	/* 7.1.  Media Type Registration in RFC 7798*/
	BYTE configurationVersion = 0;
	BYTE profileSpace = 0; // [0, 3]
	BYTE tierFlag; // [0,1]
	BYTE profileIndication = 1; // [0, 31], 1(Main) if not present
	BYTE profileCompatibilityIndication; // 32 bits flags
	BYTE levelIndication; // [0,255]
	BYTE NALUnitLength;

	// VPS
    BYTE numOfVideoParameterSets = 0; // MAX_VPS_COUNT: 16 bcoz 7.4.2.1: vps_video_parameter_set_id is u(4)
	std::vector<WORD> vpsSizes;
	std::vector<BYTE*> vpsData;
	DWORD vpsTotalSizes = 0;
	// SPS
    BYTE numOfSequenceParameterSets = 0; // MAX_SPS_COUNT: 16 bcoz 7.4.3.2.1: sps_seq_parameter_set_id is in [0, 15]
	std::vector<BYTE> sps_nul_layer_ids; // needed to Decode SPS
	std::vector<WORD> spsSizes;
	std::vector<BYTE*> spsData;
	DWORD spsTotalSizes = 0;
	// PPS
   	BYTE numOfPictureParameterSets = 0; // MAX_PPS_COUNT: 64 bcoz // 7.4.3.3.1: pps_pic_parameter_set_id is in [0, 63]
	std::vector<WORD> ppsSizes;
	std::vector<BYTE*> ppsData;
	DWORD ppsTotalSizes = 0;
};

#endif	/* HEVCDESCRIPTOR_H */

