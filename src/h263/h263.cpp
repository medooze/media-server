#include "h263.h"

static H263MCBPCEntry H263MCPBTableI[] =
{
	{0,3,{0,0},1,0x0001},
	{1,3,{0,1},3,0x0001},
	{2,3,{1,0},3,0x0002},
	{3,3,{1,1},3,0x0003},
	{4,4,{0,0},4,0x0001},
	{5,4,{0,1},6,0x0001},
	{6,4,{1,0},6,0x0002},
	{7,4,{1,1},6,0x0003},
	{8,-1,{-1,-1},9,0x0001}
};

static H263MCBPCEntry H263MCPBTableP[] =
{
	{0,0,{0,0},1,0x0001},
	{1,0,{0,1},4,0x0003},
	{2,0,{1,0},4,0x0002},
	{3,0,{1,1},6,0x0005},
	{4,1,{0,0},3,0x0003},
	{5,1,{0,1},7,0x0007},
	{6,1,{1,0},7,0x0006},
	{7,1,{1,1},9,0x0005},
	{8,2,{0,0},3,0x0002},
	{9,2,{0,1},7,0x0005},
	{10,2,{1,0},7,0x0004},
	{11,2,{1,1},8,0x0005},
	{12,3,{0,0},5,0x0003},
	{13,3,{0,1},8,0x0004},
	{14,3,{1,0},8,0x0003},
	{15,3,{1,1},7,0x0003},
	{16,4,{0,0},6,0x0004},
	{17,4,{0,1},9,0x0004},
	{18,4,{1,0},9,0x0003},
	{19,4,{1,1},9,0x0002},
	{20,-1,{-1,-1},9,0x0001},
	{21,5,{0,0},11,0x0002},
	{22,5,{0,1},13,0x000C},
	{23,5,{1,0},13,0x000E},
	{24,5,{1,1},13,0x000F}
};

static H263MVDEntry H263MVDTable[] =
{
	{0,-16,16,13,0x0005},
	{1,-15.5,16.5,13,0x0007},
	{2,-15,17,12,0x0005},
	{3,-14.5,17.5,12,0x0007},
	{4,-14,18,12,0x0009},
	{5,-13.5,18.5,12,0x000B},
	{6,-13,19,12,0x000D},
	{7,-12.5,19.5,12,0x000F},
	{8,-12,20,11,0x0009},
	{9,-11.5,20.5,11,0x000B},
	{10,-11,21,11,0x000D},
	{11,-10.5,21.5,11,0x000F},
	{12,-10,22,11,0x0011},
	{13,-9.5,22.5,11,0x0013},
	{14,-9,23,11,0x0015},
	{15,-8.5,23.5,11,0x0017},
	{16,-8,24,11,0x0019},
	{17,-7.5,24.5,11,0x001B},
	{18,-7,25,11,0x001D},
	{19,-6.5,25.5,11,0x001F},
	{20,-6,26,11,0x0021},
	{21,-5.5,26.5,11,0x0023},
	{22,-5,27,10,0x0013},
	{23,-4.5,27.5,10,0x0015},
	{24,-4,28,10,0x0017},
	{25,-3.5,28.5,8,0x0007},
	{26,-3,29,8,0x0009},
	{27,-2.5,29.5,8,0x000B},
	{28,-2,30,7,0x0007},
	{29,-1.5,30.5,5,0x0003},
	{30,-1,31,4,0x0003},
	{31,-0.5,31.5,3,0x0003},
	{32,0,0,1,0x0001},
	{33,0.5,-31.5,3,0x0002},
	{34,1,-31,4,0x0002},
	{35,1.5,-30.5,5,0x0002},
	{36,2,-30,7,0x0006},
	{37,2.5,-29.5,8,0x000A},
	{38,3,-29,8,0x0008},
	{39,3.5,-28.5,8,0x0006},
	{40,4,-28,10,0x0016},
	{41,4.5,-27.5,10,0x0014},
	{42,5,-27,10,0x0012},
	{43,5.5,-26.5,11,0x0022},
	{44,6,-26,11,0x0020},
	{45,6.5,-25.5,11,0x001E},
	{46,7,-25,11,0x001C},
	{47,7.5,-24.5,11,0x001A},
	{48,8,-24,11,0x0018},
	{49,8.5,-23.5,11,0x0016},
	{50,9,-23,11,0x0014},
	{51,9.5,-22.5,11,0x0012},
	{52,10,-22,11,0x0010},
	{53,10.5,-21.5,11,0x000E},
	{54,11,-21,11,0x000C},
	{55,11.5,-20.5,11,0x000A},
	{56,12,-20,11,0x0008},
	{57,12.5,-19.5,12,0x000E},
	{58,13,-19,12,0x000C},
	{59,13.5,-18.5,12,0x000A},
	{60,14,-18,12,0x0008},
	{61,14.5,-17.5,12,0x0006},
	{62,15,-17,12,0x0004},
	{63,15.5,-16.5,13,0x0006}
};

static H263TCOEFEntry H263TCOEFTable[] =
{
	{0,0,0,1,2,0x0002},
	{1,0,0,2,4,0x000F},
	{2,0,0,3,6,0x0015},
	{3,0,0,4,7,0x0017},
	{4,0,0,5,8,0x001F},
	{5,0,0,6,9,0x0025},
	{6,0,0,7,9,0x0024},
	{7,0,0,8,10,0x0021},
	{8,0,0,9,10,0x0020},
	{9,0,0,10,11,0x0007},
	{10,0,0,11,11,0x0006},
	{11,0,0,12,11,0x0020},
	{12,0,1,1,3,0x0006},
	{13,0,1,2,6,0x0014},
	{14,0,1,3,8,0x001E},
	{15,0,1,4,10,0x000F},
	{16,0,1,5,11,0x0021},
	{17,0,1,6,12,0x0050},
	{18,0,2,1,4,0x000E},
	{19,0,2,2,8,0x001D},
	{20,0,2,3,10,0x000E},
	{21,0,2,4,12,0x0051},
	{22,0,3,1,5,0x000D},
	{23,0,3,2,9,0x0023},
	{24,0,3,3,10,0x000D},
	{25,0,4,1,5,0x000C},
	{26,0,4,2,9,0x0022},
	{27,0,4,3,12,0x0052},
	{28,0,5,1,5,0x000B},
	{29,0,5,2,10,0x000C},
	{30,0,5,3,12,0x0053},
	{31,0,6,1,6,0x0013},
	{32,0,6,2,10,0x000B},
	{33,0,6,3,12,0x0054},
	{34,0,7,1,6,0x0012},
	{35,0,7,2,10,0x000A},
	{36,0,8,1,6,0x0011},
	{37,0,8,2,10,0x0009},
	{38,0,9,1,6,0x0010},
	{39,0,9,2,10,0x0008},
	{40,0,10,1,7,0x0016},
	{41,0,10,2,12,0x0055},
	{42,0,11,1,7,0x0015},
	{43,0,12,1,7,0x0014},
	{44,0,13,1,8,0x001C},
	{45,0,14,1,8,0x001B},
	{46,0,15,1,9,0x0021},
	{47,0,16,1,9,0x0020},
	{48,0,17,1,9,0x001F},
	{49,0,18,1,9,0x001E},
	{50,0,19,1,9,0x001D},
	{51,0,20,1,9,0x001C},
	{52,0,21,1,9,0x001B},
	{53,0,22,1,9,0x001A},
	{54,0,23,1,11,0x0022},
	{55,0,24,1,11,0x0023},
	{56,0,25,1,12,0x0056},
	{57,0,26,1,12,0x0057},
	{58,1,0,1,4,0x0007},
	{59,1,0,2,9,0x0019},
	{60,1,0,3,11,0x0005},
	{61,1,1,1,6,0x000F},
	{62,1,1,2,11,0x0004},
	{63,1,2,1,6,0x000E},
	{64,1,3,1,6,0x000D},
	{65,1,4,1,6,0x000C},
	{66,1,5,1,7,0x0013},
	{67,1,6,1,7,0x0012},
	{68,1,7,1,7,0x0011},
	{69,1,8,1,7,0x0010},
	{70,1,9,1,8,0x001A},
	{71,1,10,1,8,0x0019},
	{72,1,11,1,8,0x0018},
	{73,1,12,1,8,0x0017},
	{74,1,13,1,8,0x0016},
	{75,1,14,1,8,0x0015},
	{76,1,15,1,8,0x0014},
	{77,1,16,1,8,0x0013},
	{78,1,17,1,9,0x0018},
	{79,1,18,1,9,0x0017},
	{80,1,19,1,9,0x0016},
	{81,1,20,1,9,0x0015},
	{82,1,21,1,9,0x0014},
	{83,1,22,1,9,0x0013},
	{84,1,23,1,9,0x0012},
	{85,1,24,1,9,0x0011},
	{86,1,25,1,10,0x0007},
	{87,1,26,1,10,0x0006},
	{88,1,27,1,10,0x0005},
	{89,1,28,1,10,0x0004},
	{90,1,29,1,11,0x0024},
	{91,1,30,1,11,0x0025},
	{92,1,31,1,11,0x0026},
	{93,1,32,1,11,0x0027},
	{94,1,33,1,12,0x0058},
	{95,1,34,1,12,0x0059},
	{96,1,35,1,12,0x005A},
	{97,1,36,1,12,0x005B},
	{98,1,37,1,12,0x005C},
	{99,1,38,1,12,0x005D},
	{100,1,39,1,12,0x005E},
	{101,1,40,1,12,0x005F},
	{102,-1,-1,-1,7,0x0003}
};

static H263CPBYEntry H263TCPBYTable[] =
{
	{0,{0,0,0,0},{1,1,1,1},4,0x03},
	{1,{0,0,0,1},{1,1,1,0},5,0x05},
	{2,{0,0,1,0},{1,1,0,1},5,0x04},
	{3,{0,0,1,1},{1,1,0,0},4,0x09},
	{4,{0,1,0,0},{1,0,1,1},5,0x03},
	{5,{0,1,0,1},{1,0,1,0},4,0x07},
	{6,{0,1,1,0},{1,0,0,1},6,0x02},
	{7,{0,1,1,1},{1,0,0,0},4,0x0B},
	{8,{1,0,0,0},{0,1,1,1},5,0x02},
	{9,{1,0,0,1},{0,1,1,0},6,0x03},
	{10,{1,0,1,0},{0,1,0,1},4,0x05},
	{11,{1,0,1,1},{0,1,0,0},4,0x0A},
	{12,{1,1,0,0},{0,0,1,1},4,0x04},
	{13,{1,1,0,1},{0,0,1,0},4,0x08},
	{14,{1,1,1,0},{0,0,0,1},4,0x06},
	{15,{1,1,1,1},{0,0,0,0},2,0x03}
};

H263MCBPCIntraTableVlc::H263MCBPCIntraTableVlc()
{
	//For each entry
	for (int i=0;i<9;i++)
		//add vlc code
		AddValue(H263MCPBTableI[i].code,H263MCPBTableI[i].len,&H263MCPBTableI[i]);

}
H263MCBPCInterTableVlc::H263MCBPCInterTableVlc()
{
	//For each entry
	for (int i=0;i<25;i++)
		//add vlc code
		AddValue(H263MCPBTableP[i].code,H263MCPBTableP[i].len,&H263MCPBTableP[i]);

}

H263CPBYTableVlc::H263CPBYTableVlc()
{
	//For each entry
	for (int i=0;i<16;i++)
		//add vlc code
		AddValue(H263TCPBYTable[i].code,H263TCPBYTable[i].len,&H263TCPBYTable[i]);


}
H263MVDTableVlc::H263MVDTableVlc()
{
	//For each entry
	for (int i=0;i<64;i++)
		//add vlc code
		AddValue(H263MVDTable[i].code,H263MVDTable[i].len,&H263MVDTable[i]);
	

}
H263TCOEFTableVlc::H263TCOEFTableVlc()
{
	//For each entry
	for (int i=0;i<103;i++)
		//add vlc code
		AddValue(H263TCOEFTable[i].code,H263TCOEFTable[i].len,&H263TCOEFTable[i]);
}

H263HeadersBasic* H263HeadersBasic::CreateHeaders(BYTE first)
{
	//Check f bit
	if (first >> 7)
		return new H263HeadersModeB();
	else
		return new H263HeadersModeA();
}

H263HeadersModeA::H263HeadersModeA()
{
	//Empty
	 f = 0;
	 p = 0;
	 sbits = 0;
	 ebits = 0;
	 src = 0;
	 i = 0;
	 u = 0;
	 s = 0;
	 a = 0;
	 r = 0;
	 dbq = 0;
	 trb = 0;
	 tr = 0;
}

BYTE* H263HeadersModeA::GetData()
{
	//Writer
	BitWritter w(data,sizeof(data));
	//Process
	w.Put(1,f);
	w.Put(1,p);
	w.Put(3,sbits);
	w.Put(3,ebits);
	w.Put(3,src);
	w.Put(1,i);
	w.Put(1,u);
	w.Put(1,s);
	w.Put(1,a);
	w.Put(4,r);
	w.Put(2,dbq);
	w.Put(3,trb);
	w.Put(8,tr);
	//Flush
	w.Flush();
	//And return it
	return data;
}

DWORD H263HeadersModeA::GetSize()
{
	return sizeof(data);
}

int H263HeadersModeA::Parse(BYTE *buffer,DWORD bufferLen)
{
	if (bufferLen<sizeof(data))
		return 0;
	//Craete reader
	BitReader reader(buffer,bufferLen);
	//Parse
	f	= reader.Get(1);
	p	= reader.Get(1);
	sbits	= reader.Get(3);
	ebits	= reader.Get(3);
	src	= reader.Get(3);
	i	= reader.Get(1);
	u	= reader.Get(1);
	s	= reader.Get(1);
	a	= reader.Get(1);
	r	= reader.Get(4);
	dbq	= reader.Get(2);
	trb	= reader.Get(3);
	tr	= reader.Get(8);
	//Parsed
	return sizeof(data);
}

H263HeadersModeB::H263HeadersModeB()
{
	//Empty
	f = 1;
	p = 0;
	sbits = 0;
	ebits = 0;
	src = 0;
	quant = 0;
	gobn = 0;
	mba = 0;
	r = 0;
	i = 0;
	u = 0;
	s = 0;
	a = 0;
	hmv1 = 0;
	vmv1 = 0;
	hmv2 = 0;
	vmv2 = 0;
}

BYTE* H263HeadersModeB::GetData()
{
	//Writer
	BitWritter w(data,sizeof(data));
	//Process
	w.Put(1,f);
	w.Put(1,p);
	w.Put(3,sbits);
	w.Put(3,ebits);
	w.Put(3,src);
	w.Put(5,quant);
	w.Put(5,gobn);
	w.Put(9,mba);
	w.Put(2,r);
	w.Put(1,i);
	w.Put(1,u);
	w.Put(1,s);
	w.Put(1,a);
	w.Put(7,hmv1);
	w.Put(7,vmv1);
	w.Put(7,hmv2);
	w.Put(7,vmv2);
	//Flush
	w.Flush();
	//And return it
	return data;
}

DWORD H263HeadersModeB::GetSize()
{
	return sizeof(data);
}

int H263HeadersModeB::Parse(BYTE *buffer,DWORD bufferLen)
{
	if (bufferLen<sizeof(data))
		return 0;
	//Craete reader
	BitReader reader(buffer,bufferLen);
	//Parse
	f 	 = reader.Get(1);
	p 	 = reader.Get(1);
	sbits 	 = reader.Get(3);
	ebits 	 = reader.Get(3);
	src 	 = reader.Get(3);
	quant 	 = reader.Get(5);
	gobn 	 = reader.Get(5);
	mba 	 = reader.Get(9);
	r 	 = reader.Get(2);
	i 	 = reader.Get(1);
	u 	 = reader.Get(1);
	s 	 = reader.Get(1);
	a 	 = reader.Get(1);
	hmv1 	 = reader.Get(7);
	vmv1 	 = reader.Get(7);
	hmv2 	 = reader.Get(7);
	vmv2 	 = reader.Get(7);
	//Parsed
	return sizeof(data);
}

bool H263RFC2190Paquetizer::PaquetizeFrame(VideoFrame	*frame)
{
	//Headers
	H263HeadersModeA headerA;
	H263HeadersModeB headerB;

	//Init bit reader
	BitReader r(frame->GetData(),frame->GetLength());

	//Reset dimension
	DWORD width = 0;
	DWORD height = 0;

	//Packetization
	DWORD ini = 0;
	DWORD lastGOB = 0;
	DWORD lastMB = 0;
	DWORD pos = 0;
	BYTE numGOB = 0;
	bool firstMB = false;
	bool modeA = true;

	//PSC
	if (!r.Check(22,0x20))
		return false;

	//TemporalReference UB[8] See H.263 5.1.2
	r.Skip(8);

	//PTYPE UB[5]
	r.Skip(5);

	//PictureSize UB[3] 000: custom, 1 byte
	// 000 forbidden
	// 001 subQCIF
	// 010 QCIF
	// 011 CIF
	// 100 4CIF
	// 101 16CIF
	// 111 ext
	DWORD size = r.Get(3);

	//Set size
	headerA.src = size;
	headerB.src = size;

	switch(size)
	{
		case 0x01:
			//SQCIF
			width = 160;
			height = 120;
		case 0x02:
			//QCIF
			width = 174;
			height = 144;
			break;
		case 0x03:
			//CIF
			width = 352;
			height = 288;
			break;
		case 0x04:
			//4QCIF
			width = 352*2;
			height = 288*2;
			break;
		case 0x05:
			//4QCIF
			width = 352*4;
			height = 288*4;
			break;
		case 0x07:
			//Skip
			return Error("EPLUS");
		default:
			//Not supported yet
			return false;
	}

	//Calculate number of macroblocks
	DWORD numMB = ((width+15)/16)*((height+15)/16);

	//PTYPE Bit 9 intra
	bool isIntra = !r.Get(1);

	//Set it
	headerA.i = isIntra;
	headerB.i = isIntra;

	//Skip Bit 10 to 13
	r.Skip(4);

	//Quantizer UB[5] See H.263 5.1.4 (5.1.19)
	r.Skip(5);

	//Skip CMP 1 bit
	r.Skip(1);

	//ExtraInformationFlag UB[1] See H.263 5.1.9
	// The ExtraInformationFlag-ExtraInformation sequence repeats until an ExtraInformationFlag of 0 isencountered
	while(r.Get(1))
		// ExtraInformation If ExtraInformationFlag = 1, UB[8] Otherwise absent
		r.Skip(8);

	//First gob
	bool first = true;

	//Macroblock layer
	for(DWORD y=0;y<((height+15)/16);y++)
	{
		//If it is no first GOB
		if (!first)
		{
			//Check gob end
			if (r.Left()>16 && r.Peek(16)==0)
			{
				//Skip GSTUF  + GBSC (17 bits)
				r.Skip(16);
				//Until next 1
				while(!r.Get(1))
				{
					//Noting
				}
				//Read gob number
				numGOB = r.Get(5);

				//Check it is not EOS
				if (numGOB==0x1F)
					//Exit
					return true;

				//Skip GFID 2 bits
				r.Skip(2);

				//Ski GQUANT 5 bits
				r.Skip(5);

			} else {
				//More
				numGOB++;
			}
			//Debug("Gob %d\n",numGOB);
		} else {
			//Debug("Gob 0\n");
			//Not first
			first = false;
		}

		//For each row
		for (DWORD x=0;x<((width+15)/16);x++)
		{
			//Debug(">MACROBLOCK [%d,%d]\n",mb,pos);
			//MB has nothing by default
			bool hasCPBY = false;
			bool hasDQUANT = false;
			bool hasMVD = false;
			bool hasMVD24 = false;
			bool hasINTRADC = false;
			bool isMBIntra = isIntra;

			BYTE cbp[6];
			//Not coded
			cbp[0] = cbp[1] = cbp[2] = cbp[3] = cbp[4] = cbp[5] = 0;

			//Check MB type
			if (isIntra)
			{
				H263MCBPCEntry* mcbp = NULL;

				//Skip stuffing
				do {
					//Get Macroblock type & codec block pattern for I-Pictures
					mcbp = vlcMCPBI.Decode(r);
					//Check if not error
					if (!mcbp)
						//Error
						return false;
					//Debug
					//mcbp->Dump();
				} while (mcbp->index==8);

				//Depending on the type
				switch(mcbp->type)
				{
					case (BYTE)-1:
						//Stufing
						continue;
					case 3:
						//INTRA
						hasCPBY = true;
						hasINTRADC = true;
						break;
					case 4:
						//INTRA+Q
						hasCPBY = true;
						hasDQUANT = true;
						hasINTRADC = true;
						break;
				}
				//Get crominance pattern
				cbp[4] = mcbp->cbpc[0];
				cbp[5] = mcbp->cbpc[1];
			} else {
				//CodedMacroblockFlag UB[1] See H.263 5.3.1 If 1, macro block ends here
				BYTE coded = r.Get(1);
				//Check it has more info for this MB
				if (coded)
					//Another MB
					continue;

				H263MCBPCEntry* mcbp = NULL;

				//Skip stuffing
				do {
					//Get Macroblock type & codec block pattern for I-Pictures
					mcbp = vlcMCPBP.Decode(r);
					//Check if not error
					if (!mcbp)
						//Error
						return false;
					//Debug
					//mcbp->Dump();
				} while (mcbp->index==20);

				//Depending on the type
				switch(mcbp->type)
				{
					case (BYTE)-1:
						//Stuffing
						continue;
					case 0:
						//INTER
						hasCPBY = true;
						hasMVD = true;
						break;
					case 1:
						//INTER+Q
						hasCPBY = true;
						hasDQUANT = true;
						hasMVD = true;
						break;
					case 2:
						//INTER+4V
						hasCPBY = true;
						hasMVD = true;
						hasMVD24 = true;
						break;
					case 3:
						//INTRA
						hasCPBY = true;
						hasINTRADC = true;
						isMBIntra = true;
						break;
					case 4:
						//INTRA+QUANT
						hasCPBY = true;
						hasDQUANT = true;
						hasINTRADC = true;
						isMBIntra = true;
						break;
					case 5:
						//INTRA+QUANT
						hasCPBY = true;
						hasDQUANT = true;
						hasMVD = true;
						hasMVD24 = true;
						isMBIntra = true;
						break;
				}
				//Get crominance pattern
				cbp[4] = mcbp->cbpc[0];
				cbp[5] = mcbp->cbpc[1];
			}

			//Check if it has CPBY
			if (hasCPBY)
			{
				//Debug("hasCPBY\n");
				//Get the CBPY variable code
				H263CPBYEntry* cpby = vlcCPBY.Decode(r);
				//If not found
				if (!cpby)
					//Error
					return false;
				//Debug
				//cpby->Dump();
				//Get block pattern for crominance
				if (isMBIntra)
				{
					//Get luminance pattern
					cbp[0] = cpby->cbpyI[0];
					cbp[1] = cpby->cbpyI[1];
					cbp[2] = cpby->cbpyI[2];
					cbp[3] = cpby->cbpyI[3];
				} else {
					//Get luminance pattern
					cbp[0] = cpby->cbpyP[0];
					cbp[1] = cpby->cbpyP[1];
					cbp[2] = cpby->cbpyP[2];
					cbp[3] = cpby->cbpyP[3];
				}
			}

			//Check if it has DQUant
			if (hasDQUANT)
			{
				//Debug("hasDQUANT\n");
				//Get the DQUANT
				DWORD dquant = r.Get(2);
			}

			//Check if it has CPBY codec
			if (hasMVD)
			{
				//Debug("hasMVD\n");
				//Get the MVD variable code horizontal
				H263MVDEntry* mvd = vlcMVD.Decode(r);
				//If not found
				if (!mvd)
					//Error
					return false;
				//If not found
				if (!mvd)
					//Error
					return false;
				//Get the MVD variable code vertival
				mvd = vlcMVD.Decode(r);
				//If not found
				if (!mvd)
					//Error
					return false;
			}

			//Check if it has CPBY codec
			if (hasMVD24)
			{
				//("hasMVD24\n");
				//Get all 6 values
				for (int i=0;i<6;i++)
				{
					//Get the MVD variable code 2
					H263MVDEntry* mvd = vlcMVD.Decode(r);
					//If not found
					if (!mvd)
						//Error
						return false;
					//Debug
					//mvd->Dump();
				}
			}

			//Get 4 blocks for luminance and 2 for crominance
			for (int i=0;i<6;i++)
			{

				//Check type
				if (hasINTRADC)
				{
					//Debug("hasINTRADC\n");
					//Get INTRADC
					r.Skip(8);
				}
				//Check if it is coded according to the block patterh
				if (cbp[i])
				{
					//Debug("cbp[%d]\n",i);
					BYTE last = 0;
					//Process until last
					while(!last)
					{
						//Get next
						H263TCOEFEntry* tcoef = vlcTCOEF.Decode(r);
						//If not found
						if (!tcoef)
							//Error
							return false;
						//Debug
						//tcoef->Dump();
						//Check if it is the escape code
						if (tcoef->last==(BYTE)-1)
						{
							//Get last mark
							last = r.Get(1);
							//Copy RUN (6 bits) and level (8 bits);
							r.Skip(14);
						} else {
							//Skip sign
							r.Skip(1);
							//Update last mark
							last = tcoef->last;
						}

					}
				}
			}
			//Debug("<MACROBLOCK [pos:%d,size:%d]\n",r.GetPos(),r.GetPos()-pos);
			//We are not the first anymore
			firstMB = false;

			//Get current position
			pos = r.GetPos()-1;

			//Check the size of the current paquet
			while (pos-ini>RTPPAYLOADSIZE*8)
			{
				//Check the mode
				if (modeA)
				{
					//Check if the we have a full GOB
					if (lastGOB>ini)
					{
						//Get start and end bytes
						BYTE ebit = (8 - (lastGOB+1 % 8)) % 8;
						BYTE sbit = ini % 8;
						//Get size in bits
						DWORD packsetSize = lastGOB/8 - ini/8 + 1;
						//Set it
						headerA.sbits = sbit;
						headerA.ebits = ebit;
						//Add RTP packet
						frame->AddRtpPacket(ini/8, packsetSize, headerA.GetData(), headerA.GetSize());
						//Move the begining of the last gob
						ini = lastGOB+1;
					} else { 
						//We are not in mode A anymore
						modeA = false;
					}
				} else {
					//Get start and end bytes
					BYTE ebit = (8 - (lastMB+1 % 8)) % 8;
					BYTE sbit = ini % 8;
					//Get size in bits
					DWORD packsetSize = lastMB/8 - ini/8 + 1;
					//Set it
					headerB.sbits = sbit;
					headerB.ebits = ebit;
					//Add RTP packet
					frame->AddRtpPacket(ini/8, packsetSize, headerB.GetData(), headerB.GetSize());
					//Move the begining of the last MB
					ini = lastMB+1;
					//Next is the first one
					firstMB = true;
				}
			}
			//Set last MB position
			lastMB = pos;
		}
		//Set last GOB position
		lastGOB = r.GetPos()-1;
		//Check if it is the last MB of a mode B packet
		if(!modeA)
		{
			//Get start and end bytes
			BYTE ebit = (8 - (lastMB+1 % 8)) % 8;
			BYTE sbit = ini % 8;
			//Get size in bits
			DWORD packsetSize = lastMB/8 - ini/8 + 1;
			//Set it
			headerB.sbits = sbit;
			headerB.ebits = ebit;
			//Add RTP packet
			frame->AddRtpPacket(ini/8, packsetSize, headerB.GetData(), headerB.GetSize());
			//Move the begining of the last MB
			ini = lastMB+1;
			//Next is the first one
			firstMB = true;
			//We are mode A again
			modeA = true;
		}
	}

	//Get current pos
	pos = r.GetPos()-1;

	//Check size
	if (pos>ini)
	{
		//Get start and end bytes
		BYTE ebit = (8 - (pos+1 % 8)) % 8;
		BYTE sbit = ini % 8;
		//Get size in bits
		DWORD packsetSize = pos/8 - ini/8 + 1;
		//Check the mode
		if (modeA)
		{
			//Set start and end bits
			headerA.sbits = sbit;
			headerA.ebits = ebit;
			//Add RTP packet
			frame->AddRtpPacket(ini/8, packsetSize, headerA.GetData(), headerA.GetSize());
		} else {
			//Set start and end bits
			headerB.sbits = sbit;
			headerB.ebits = ebit;
			//Add RTP packet
			frame->AddRtpPacket(ini/8, packsetSize, headerB.GetData(), headerB.GetSize());
		}
	}
	//Everything was good
	return true;
}
