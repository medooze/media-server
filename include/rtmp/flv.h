#ifndef _FLV_H_
#define _FLV_H_
#include "rtmp.h"
#include "amf.h"

class FLVHeader : public RTMPFormat<9>
{
public:
	BYTE*   GetTag()        	{ return data;	       }
        BYTE    GetVersion()  		{ return get1(data,3); }
        BYTE    GetMedia()      	{ return get1(data,4); }
        BYTE    GetHeaderOffset()	{ return get4(data,5); }

	void 	SetTag(BYTE* tag)	{ memcpy(data,tag,3);	}
        void    SetVersion(BYTE version){ set1(data,3,version);	}
        void    SetMedia(BYTE media)   	{ set1(data,4,media); 	}
        void    SetHeaderOffset(DWORD o){ set4(data,5,o); 	}

        virtual void Dump()
	{
                Debug("[FLVHeader tag=%.2x%.2x%.2x version=%d media=%d offset=%d/]\n",data[0],data[1],data[2],GetVersion(),GetMedia(),GetHeaderOffset());
	}
};

class FLVTag : public RTMPFormat<11>
{
public:
	BYTE    GetType()        	{ return get1(data,0); }
        DWORD   GetDataSize()  		{ return get3(data,1); }
        DWORD   GetTimestamp()      	{ return get3(data,4); }
        BYTE    GetTimestampExt()	{ return get1(data,7); }
        DWORD   GetStreamId()		{ return get3(data,8); }

	void    SetType(BYTE type)    	 { set1(data,0,type);	}
        void    SetDataSize(DWORD size)  { set3(data,1,size);	}
        void    SetTimestamp(DWORD ts)   { set3(data,4,ts);	}
        void    SetTimestampExt(BYTE ext){ set1(data,7,ext);	}
        void    SetStreamId(DWORD id)	 { set3(data,8,id);	}

        virtual void Dump()
	{
                Debug("[FLVTag type=%d size=%d timeStamp=%d timeStampExt=%d streamId=%d/]\n",GetType(),GetDataSize(),GetTimestamp(),GetTimestampExt(),GetStreamId());
	}
};

class FLVTagSize : public RTMPFormat<4>
{
public:
        DWORD   GetTagSize()  		{ return get4(data,0);	}
        void    SetTagSize(DWORD size)	{ set4(data,0,size);	}
        virtual void Dump()
	{
                Debug("[FLVTagSize size=%d/]\n",GetTagSize());
	}
};

#endif
