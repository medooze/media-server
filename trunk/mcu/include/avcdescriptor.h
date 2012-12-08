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

class AVCDescriptor
{
public:
	AVCDescriptor();
	~AVCDescriptor();
	void AddSequenceParameterSet(BYTE *data,DWORD size);
	void AddPictureParameterSet(BYTE *data,DWORD size);
	void AddParametersFromFrame(BYTE *data,DWORD size);
	void ClearSequenceParameterSets();
	void ClearPictureParameterSets();
	DWORD Serialize(BYTE* buffer,DWORD bufferLength) const;
	bool Parse(BYTE* data,DWORD size);
	void Dump() const;

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
	BYTE GetNALUnitLength()			const { return NALUnitLength;			}
	DWORD GetSize()				const { return 7+2*(numOfSequenceParameterSets+numOfPictureParameterSets)+ppsTotalSizes+spsTotalSizes;	}
	void SetAVCLevelIndication(BYTE AVCLevelIndication)		{ this->AVCLevelIndication = AVCLevelIndication;	}
	void SetProfileCompatibility(BYTE profileCompatibility)		{ this->profileCompatibility = profileCompatibility;	}
	void SetAVCProfileIndication(BYTE AVCProfileIndication)		{ this->AVCProfileIndication = AVCProfileIndication;	}
	void SetConfigurationVersion(BYTE configurationVersion)		{ this->configurationVersion = configurationVersion;	}
	void SetNALUnitLength(BYTE NALUnitLength)			{ this->NALUnitLength = NALUnitLength;			}


private:
	BYTE configurationVersion;
	BYTE AVCProfileIndication;
	BYTE profileCompatibility;
	BYTE AVCLevelIndication;
	BYTE NALUnitLength;
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

