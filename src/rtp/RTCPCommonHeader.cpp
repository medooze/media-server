#include "rtp/RTCPCommonHeader.h"
#include "DTLSICETransport.h"
#include "log.h"

RTCPCommonHeader::RTCPCommonHeader()
{
	version		= 2;
	padding		= 0;
	count		= 0;
	packetType	= 0;
	length		= 0;
	
}
/*
 
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT          |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 
 
 
 */
DWORD RTCPCommonHeader::Parse(const BYTE* data,const DWORD size)
{
	//Ensure minumim size
	if (size<4)
		return 0;
	
	//Get data
	version		= data[0] >> 6;
	padding		= (data[0] >> 4 ) & 0x01;
	count		= data[0] & 0x1F;
	packetType	= data[1];
	length		= (get2(data,2)+1)*4;
	
	//Return size
	return 4;
}

DWORD RTCPCommonHeader::Serialize(BYTE* data, const DWORD size) const
{
	//Check size
	if (size<4)
		//Error
		return 0;
	
	//Set data
	data[0] = (padding ? 0xA0 : 0x80) | (count & 0x1F);
	data[1] = packetType;
	set2(data,2, (length/4)-1);
	
	//Return size
	return 4;
	
}

void RTCPCommonHeader::Dump() const
{
	Debug("[RTCPCommonHeader v=%d p=%d x=%d cc=%d pt=%d len=%d/]\n",
		version,
		padding,
		count,
		packetType,
		length
	);
}