/* 
 * File:   AVCDescriptor.h
 * Author: Sergio
 *
 * Created on 21 de junio de 2011, 12:21
 */

#ifndef AVCDESCRIPTOR_H
#define	AVCDESCRIPTOR_H

#include "config.h"
#include <vector>

#include "CodecConfigurationRecord.h"
#include "h264/h264.h"

class AVCDescriptor : public CodecConfigurationRecord
{
public:
	AVCDescriptor();
	~AVCDescriptor();
	void AddSequenceParameterSet(const BYTE *data,DWORD size);
	void AddPictureParameterSet(const BYTE *data,DWORD size);
	void AddParametersFromFrame(const BYTE *data,DWORD size);
	void ClearSequenceParameterSets();
	void ClearPictureParameterSets();
	DWORD Serialize(BYTE* buffer,DWORD bufferLength) const;
	bool Parse(const BYTE* data,DWORD size);
	void Dump() const;

	// For compatibility with RTMP packetizer. Not used for AVC
	BYTE GetNumOfVideoParameterSets()		const { return 0;	}	
	BYTE* GetVideoParameterSet(BYTE i)		const { return nullptr;	}
	DWORD GetVideoParameterSetSize(BYTE i)		const { return 0;	}

	BYTE GetNumOfPictureParameterSets()		const { return numOfPictureParameterSets;	}
	BYTE GetNumOfSequenceParameterSets()		const { return numOfSequenceParameterSets;	}
	
	BYTE* GetPictureParameterSet(BYTE i)		const { return ppsData[i];	}
	DWORD GetPictureParameterSetSize(BYTE i)	const { return ppsSizes[i];	}
	BYTE* GetSequenceParameterSet(BYTE i)		const { return spsData[i];	}
	DWORD GetSequenceParameterSetSize(BYTE i)	const { return spsSizes[i];	}
	
	BYTE GetAVCLevelIndication()		const { return AVCLevelIndication;		}
	BYTE GetProfileCompatibility()		const { return profileCompatibility;		}
	BYTE GetAVCProfileIndication()		const { return AVCProfileIndication;		}
	BYTE GetConfigurationVersion()		const { return configurationVersion;		}
	BYTE GetNALUnitLengthSizeMinus1()			const { return NALUnitLengthSizeMinus1;			}
	DWORD GetSize()				const { return 7+2*(numOfSequenceParameterSets+numOfPictureParameterSets)+ppsTotalSizes+spsTotalSizes;	}
	void SetAVCLevelIndication(BYTE AVCLevelIndication)		{ this->AVCLevelIndication = AVCLevelIndication;	}
	void SetProfileCompatibility(BYTE profileCompatibility)		{ this->profileCompatibility = profileCompatibility;	}
	void SetAVCProfileIndication(BYTE AVCProfileIndication)		{ this->AVCProfileIndication = AVCProfileIndication;	}
	void SetConfigurationVersion(BYTE configurationVersion)		{ this->configurationVersion = configurationVersion;	}
	void SetNALUnitLengthSizeMinus1(BYTE NALUnitLengthSizeMinus1)			{ this->NALUnitLengthSizeMinus1 = NALUnitLengthSizeMinus1;			}


	virtual bool GetResolution(unsigned& width, unsigned& height) const
	{
		for (int i=0;i<GetNumOfSequenceParameterSets();i++)
		{
			H264SeqParameterSet sps;
			if (sps.Decode(GetSequenceParameterSet(i)+1, GetSequenceParameterSetSize(i)-1))
			{
				//Set dimensions
				width = sps.GetWidth();
				height = sps.GetHeight();
				
				return true;
			}
		}
		
		return false;
	}
	
	virtual void Map(const std::function<void (const uint8_t*, size_t)>& func) override
	{
		for (int i=0;i<GetNumOfSequenceParameterSets();i++)
		{
			func(GetSequenceParameterSet(i),GetSequenceParameterSetSize(i));
		}
		
		for (int i=0;i<GetNumOfPictureParameterSets();i++)
		{
			func(GetPictureParameterSet(i),GetPictureParameterSetSize(i));
		}
	}
		
private:
	BYTE configurationVersion;
	BYTE AVCProfileIndication;
	BYTE profileCompatibility;
	BYTE AVCLevelIndication;
	BYTE NALUnitLengthSizeMinus1;
	BYTE numOfSequenceParameterSets;
	std::vector<WORD> spsSizes;
	std::vector<BYTE*> spsData;
   	BYTE numOfPictureParameterSets;
	std::vector<WORD> ppsSizes;
	std::vector<BYTE*> ppsData;
	DWORD ppsTotalSizes;
	DWORD spsTotalSizes;
};

#endif	/* AVCDESCRIPTOR_H */

