/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPReport.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:56
 */

#ifndef RTCPREPORT_H
#define RTCPREPORT_H

#include "config.h"
#include "tools.h"
#include "log.h"
#include <memory>

class RTCPReport
{
public:
	using shared = std::shared_ptr<RTCPReport>;
	
public:
	DWORD GetSSRC()			const { return get4(buffer,0);  }
	BYTE  GetFactionLost()		const { return get1(buffer,4);  }
	DWORD GetLostCount()		const { return get3(buffer,5) & 0x7FFFFF;  }
	DWORD GetLastSeqNum()		const { return get4(buffer,8);  }
	DWORD GetJitter()		const { return get4(buffer,12); }
	DWORD GetLastSR()		const { return get4(buffer,16); }
	DWORD GetDelaySinceLastSR()	const { return get4(buffer,20); }
	DWORD GetSize()			const { return 24;		}

	void SetSSRC(DWORD ssrc)		{ set4(buffer,0,ssrc);		}
	void SetFractionLost(BYTE fraction)	{ set1(buffer,4,fraction);	}
	void SetLostCount(DWORD count)		{ set3(buffer,5,count & 0x7FFFFF);		}
	void SetLastSeqNum(DWORD seq)		{ set4(buffer,8,seq);		}
	void SetLastJitter(DWORD jitter)	{ set4(buffer,12,jitter);	}
	void SetLastSR(DWORD last)		{ set4(buffer,16,last);		}
	void SetDelaySinceLastSR(DWORD delay)	{ set4(buffer,20,delay);	}

	void SetDelaySinceLastSRMilis(DWORD milis)
	{
		//calculate the delay, expressed in units of 1/65536 seconds
		DWORD dlsr = (milis/1000) << 16 | (DWORD)((milis%1000)*65.535);
		//Set it
		SetDelaySinceLastSR(dlsr);
	}

	DWORD GetDelaySinceLastSRMilis()
	{
		//Get the delay, expressed in units of 1/65536 seconds
		DWORD dslr = GetDelaySinceLastSR();
		//Return in milis
		return (dslr>>16)*1000 + ((double)(dslr & 0xFFFF))/65.635;
	}


	DWORD Serialize(BYTE* data,DWORD size)
	{
		//Check size
		if (size<24)
			return 0;
		//Copy
		memcpy(data,buffer,24);
		//Return size
		return 24;
	}

	DWORD Parse(const BYTE* data,DWORD size)
	{
		//Check size
		if (size<24)
			return 0;
		//Copy
		memcpy(buffer,data,24);
		//Return size
		return 24;
	}
	void Dump()
	{
		Debug("\t\t[Report ssrc=%u\n",		GetSSRC());
		Debug("\t\t\tfractionLost=%u\n",	GetFactionLost());
		Debug("\t\t\tlostCount=%u\n",		GetLostCount());
		Debug("\t\t\tlastSeqNum=%u\n",		GetLastSeqNum());
		Debug("\t\t\tjitter=%u\n",		GetJitter());
		Debug("\t\t\tlastSR=%u\n",		GetLastSR());
		Debug("\t\t\tdelaySinceLastSR=%u\n",	GetDelaySinceLastSR());
		Debug("\t\t/]\n");
	}
private:
	BYTE buffer[24] = {0};
};


#endif /* RTCPREPORT_H */

