/* 
 * File:   h263.h
 * Author: Sergio
 *
 * Created on 21 de noviembre de 2011, 9:41
 */

#ifndef H263_H
#define	H263_H

#include "config.h"
#include "log.h"
#include "bitstream.h"
#include "video.h"

struct H263MCBPCEntry
{
	BYTE index;
	BYTE type;
	BYTE cbpc[2];
	BYTE len;
	DWORD code;

	void Dump()
	{
		Debug("H263MCBPCEntry [index:%d,type:%d,cbpc:{%d,%d},len:%d,code:0x%x]\n",index,type,cbpc[0],cbpc[1],len,code);
	}
};

struct H263MVDEntry
{
	BYTE index;
	float vector;
	float difference;
	BYTE len;
	DWORD code;

	void Dump()
	{
		Debug("H263MVDEntry [index:%d,vector:%f,difference:%f,len:%d,code:0x%x]\n",index,vector,difference,len,code);
	}
};

struct H263TCOEFEntry
{
	BYTE index;
	BYTE last;
	BYTE run;
	BYTE level;
	BYTE len;
	DWORD code;

	void Dump()
	{
		Debug("H263TCOEFEntry [index:%d,last:%d,run:%d,level:%d,len:%d,code:0x%x]\n",index,last,run,level,len,code);
	}
};

struct H263CPBYEntry
{
	BYTE index;
	BYTE cbpyI[4];
	BYTE cbpyP[4];
	BYTE len;
	DWORD code;

	void Dump()
	{
		Debug("H263CPBYEntry [index:%d,cbpI:{%d,%d,%d,%d},cbpP:{%d,%d,%d,%d},len:%d,code:0x%x]\n",index,cbpyI[0],cbpyI[1],cbpyI[2],cbpyI[3],cbpyP[0],cbpyP[1],cbpyP[2],cbpyP[3],len,code);
	}
};

class H263MCBPCIntraTableVlc	: public VLCDecoder<H263MCBPCEntry*>	{ public: H263MCBPCIntraTableVlc();	};
class H263MCBPCInterTableVlc	: public VLCDecoder<H263MCBPCEntry*>	{ public: H263MCBPCInterTableVlc();	};
class H263CPBYTableVlc		: public VLCDecoder<H263CPBYEntry*>	{ public: H263CPBYTableVlc();		};
class H263MVDTableVlc		: public VLCDecoder<H263MVDEntry*>	{ public: H263MVDTableVlc();		};
class H263TCOEFTableVlc		: public VLCDecoder<H263TCOEFEntry*>	{ public: H263TCOEFTableVlc();		};

class H263HeadersBasic
{
public:

	//	F=0 		F=1
	//P=0	I/P frame	I/P mode b
	//P=1	B frame		B fame mode C
	BYTE f;		// size 1;
	BYTE p;		// size 1;
	BYTE sbits;	// size 3;
	BYTE ebits;	// size 3;
	BYTE src;	// size 3;
	BYTE i;		// size 1;
	BYTE u;		// size 1;
	BYTE s;		// size 1;
	BYTE a;		// size 1;
public:
	static H263HeadersBasic* CreateHeaders(BYTE first);
	virtual int Parse(BYTE *data,DWORD size) = 0;
};

class H263HeadersModeA : public H263HeadersBasic
{
public:
	H263HeadersModeA();
	BYTE* GetData();
	DWORD GetSize();
	virtual int Parse(BYTE *buffer,DWORD bufferLen);
public:
	BYTE r;		// size 4;
	BYTE dbq;	// size 2;
	BYTE trb;	// size 3;
	BYTE tr;	// size 8;
private:
	BYTE		data[4];
};

class H263HeadersModeB : public H263HeadersBasic
{
public:
	H263HeadersModeB();
	BYTE* GetData();
	DWORD GetSize();
	virtual int Parse(BYTE *buffer,DWORD bufferLen);
public:
/**********
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |F|P|SBIT |EBIT | SRC | QUANT   |  GOBN   |   MBA           |R  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |I|U|S|A| HMV1        | VMV1        | HMV2        | VMV2        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 ************/

	BYTE quant;	// size 5;
	BYTE gobn;	// size 5;
	WORD mba;	// size 9;
	BYTE r;		// size 2;
	BYTE hmv1;	// size 7;
	BYTE vmv1;	// size 7;
	BYTE hmv2;	// size 7;
	BYTE vmv2;	// siz 7;

private:
	BYTE		data[8];
};


static H263MVDEntry* getMVDByVector(H263MVDEntry table[],float val)
{
	for (int i=0;i<64;i++)
		if (table[i].vector==val)
			return &table[i];
	return NULL;
}

static H263MVDEntry* getMVDByDifference(H263MVDEntry table[],float val)
{
	for (int i=0;i<64;i++)
		if (table[i].difference==val)
			return &table[i];
	return NULL;
}

static void copyUMV(BitReader &r,BitWritter &w)
{
	//Write first and check 0 diff umv
	if (w.Put(1,r))
		//Exit
		return;

	//Check for (0,5,0,5) i.e 6 consecutive ceros
	if (r.Peek(5)==0)
	{
		//Skip
		r.Skip(5);
		//Write it
		w.Put(5,r);
		//Exit
		return;
	}

	//Copy next bit value
	w.Put(1,r);

	//Copy until a 0 mark is reached
	while (w.Put(1,r))
		//Copy next bit value
		w.Put(1,r);
}

static DWORD encodeUMV(BitWritter &w,int val)
{
	//Check
	if (val == 0)
		return w.Put(1, 1);
	if (val == 1)
		return w.Put(3, 0);
	if (val == -1)
		return w.Put(3, 2);

	short sval = ((val < 0) ? (short)(-val):(short)val);;
	short n_bits = 0;
	short temp_val = sval;
	int code = 0;

	while (temp_val != 0)
	{
	    temp_val = temp_val >> 1;
	    n_bits++;
	}

	short i = n_bits - 1;

	while (i > 0)
	{
	    int tcode = (sval & (1 << (i-1))) >> (i-1);
	    tcode = (tcode << 1) | 1;
	    code = (code << 2) | tcode;
	    i--;
	}
	code = ((code << 1) | (val < 0)) << 1;

	//Finally write it
	return w.Put((2*n_bits)+1, code);
}

class H263RFC2190Paquetizer
{
public:
	bool PaquetizeFrame(VideoFrame	*frame);
private:
	H263MCBPCIntraTableVlc	vlcMCPBI;
	H263MCBPCInterTableVlc	vlcMCPBP;
	H263CPBYTableVlc	vlcCPBY;
	H263MVDTableVlc		vlcMVD;
	H263TCOEFTableVlc	vlcTCOEF;
};

#endif	/* H263_H */

