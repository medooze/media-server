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
	void AddSequenceParameterSet(const BYTE *data,DWORD size);
	void AddPictureParameterSet(const BYTE *data,DWORD size);
	void AddParametersFromFrame(const BYTE *data,DWORD size);
	void ClearSequenceParameterSets();
	void ClearPictureParameterSets();
	DWORD Serialize(BYTE* buffer,DWORD bufferLength) const;
	bool Parse(const BYTE* data,DWORD size);
	void Dump() const;

	BYTE GetNumOfPictureParameterSets()		const { return numOfPictureParameterSets;	}
	BYTE GetNumOfSequenceParameterSets()		const { return numOfSequenceParameterSets;	}
	
	BYTE* GetPictureParameterSet(BYTE i)		const { return ppsData[i];	}
	DWORD GetPictureParameterSetSize(BYTE i)	const { return ppsSizes[i];	}
	BYTE* GetSequenceParameterSet(BYTE i)		const { return spsData[i];	}
	DWORD GetSequenceParameterSetSize(BYTE i)	const { return spsSizes[i];	}
	
	BYTE GetHEVCLevelIndication()		const { return HEVCLevelIndication;		}
	BYTE GetProfileCompatibility()		const { return profileCompatibility;		}
	BYTE GetHEVCProfileIndication()		const { return HEVCProfileIndication;		}
	BYTE GetConfigurationVersion()		const { return configurationVersion;		}
	BYTE GetNALUnitLength()			const { return NALUnitLength;			}
	DWORD GetSize()				const { return 7+2*(numOfSequenceParameterSets+numOfPictureParameterSets)+ppsTotalSizes+spsTotalSizes;	}
	void SetHEVCLevelIndication(BYTE HEVCLevelIndication)		{ this->HEVCLevelIndication = HEVCLevelIndication;	}
	void SetProfileCompatibility(BYTE profileCompatibility)		{ this->profileCompatibility = profileCompatibility;	}
	void SetHEVCProfileIndication(BYTE HEVCProfileIndication)		{ this->HEVCProfileIndication = HEVCProfileIndication;	}
	void SetConfigurationVersion(BYTE configurationVersion)		{ this->configurationVersion = configurationVersion;	}
	void SetNALUnitLength(BYTE NALUnitLength)			{ this->NALUnitLength = NALUnitLength;			}


private:
	BYTE configurationVersion;
	BYTE HEVCProfileIndication;
	BYTE profileCompatibility;
	BYTE HEVCLevelIndication;
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

#endif	/* HEVCDESCRIPTOR_H */

