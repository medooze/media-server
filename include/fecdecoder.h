/* 
 * File:   fecdecoder.h
 * Author: Sergio
 *
 * Created on 6 de febrero de 2013, 10:30
 */

#ifndef FECDECODER_H
#define	FECDECODER_H

#include "config.h"
#include "rtp.h"
#include <map>

class FECData
{

public:
	FECData()
	{
		size = 0;
		cycles = 0;
	}
	
	FECData(BYTE* data,DWORD size)
	{
		//Copy data
		memcpy(this->data,data,size);
		//Set size
		this->size = size;
		//No cycles
		cycles = 0;
	}
	
	/*
		The FEC header is 10 octets. The format of the header is shown in
		Figure 3 and consists of extension flag (E bit), long-mask flag (L
		bit), P recovery field, X recovery field, CC recovery field, M
		recovery field, PT recovery field, SN base field, TS recovery field,
		and length recovery field.
		0                   1                   2                   3
		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|E|L|P|X|   CC  |M| PT recovery |       SN base                 |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                            TS recovery                        |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| length recovery               |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+++
	 */
	bool  GetExtensionFlag()	{ return data[0] & 0x80;	}
	bool  GetLongMask()		{ return data[0] & 0x40;	}
	bool  GetRecoveryP()		{ return data[0] & 0x20;	}
	bool  GetRecoveryX()		{ return data[0] & 0x10;	}
	BYTE  GetRecoveryCC()		{ return data[0] & 0x0F;	}
	bool  GetRecoveryM()		{ return data[1] & 0x80;	}
	BYTE  GetRecoveryType()		{ return data[1] & 0x7F; 	}
	WORD  GetBaseSeqNum()		{ return get2(data,2);		}
	DWORD GetRecoveryTimestamp()	{ return get4(data,4);		}
	WORD  GetRecoveryLength()	{ return get2(data,8);		}
	DWORD GetBaseExtSeq()		{ return ((DWORD)cycles) <<16 | GetBaseSeqNum(); }
	DWORD GetBaseSeqCylcles()	{ return cycles;		}
	void  SetSeqCycles(WORD cycles)	{ this->cycles = cycles;}

	/*
	 *	The FEC level header is 4 or 8 octets (depending on the L bit in the
		FEC header). The formats of the headers are shown in Figure 4.
		The FEC level headers consist of a protection length field and a mask
		field. The protection length field is 16 bits long. The mask field
		is 16 bits long (when the L bit is not set) or 48 bits long (when the
		L bit is set).

		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| Protection Length             |              mask             |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| mask cont. (present only when L = 1)                          |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 */
	BYTE*	GetLevel0Data()		{ return GetLongMask() ? data+18: data+14;	}
	DWORD	GetLevel0Size()		{ return get2(data,10);				}
	QWORD	GetLevel0Mask()
	{
		//Get first part of the mask and shift it to the left
		QWORD mask = ((QWORD)get2(data,12)) << 48;
		//If it is long mask
		if (GetLongMask())
			//Append the rest
			mask |= ((QWORD)get4(data,14)) << 16;
		//REturn it
		return mask;
	}
	bool	IsProtectedAtLevel0(int seq)
	{
		//Check if can be check by mask
		if (seq<GetBaseSeqNum() || seq>GetBaseSeqNum()+48)
			//Not possible
			return false;
		//Check bit for the seq
		BYTE diff = seq-GetBaseSeqNum();
		//GEt mask
		QWORD mask = GetLevel0Mask();
		//Check if mask has the "diff" bit on
		return ( mask >> (64-diff-1)) & 1;
	}
	
private:
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD size;
	WORD  cycles;
};
class FECDecoder
{
public:
	FECDecoder();
	~FECDecoder();
	bool AddPacket(RTPPacket* packet);
	RTPPacket* Recover();
private:
	typedef std::map<DWORD,RTPPacket*> RTPOrderedPackets;
	typedef std::multimap<DWORD,FECData*> FECOrderedData;
private:
	RTPOrderedPackets	medias;
	FECOrderedData		codes;
};

#endif	/* FECDECODER_H */

