#include "rtp/RTPHeader.h"
#include "bitstream.h"
#include "log.h"

RTPHeader::RTPHeader()
{
	version = 2;
	padding = false;
	extension = false;
	mark  = false;
	payloadType = 0;
	sequenceNumber = 0;
	timestamp = 0;
	ssrc = 0;
}
/*
 
     0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 
 
 
 */
DWORD RTPHeader::Parse(const BYTE* data,const DWORD size)
{
	//Ensure minumim size
	if (size<12)
		return 0;
	
	//Bite reader
	BitReader r(data,2); 
		
	//Get data
	version		= r.Get(2);
	padding		= r.Get(1);
	extension	= r.Get(1);
	BYTE cc		= r.Get(4);
	mark		= r.Get(1);
	payloadType	= r.Get(7);
	
	//Get sequence number
	sequenceNumber = get2(data,2);
	
	//Get timestamp
	timestamp = get4(data,4);
	
	//Get ssrc
	ssrc = get4(data,8);
	
	//Ensure size
	if (size<12+cc*4)
		//Error
		return 0;
	
	//Get all csrcs
	for (int i=0;i<cc;++i)
		//Get them
		csrcs.push_back(get4(data,12+i*4));
	
	//Return size
	return 12+csrcs.size()*4;
}

DWORD RTPHeader::Serialize(BYTE* data, const DWORD size) const
{
	//Check size
	if (size<GetSize())
		//Error
		return 0;
	
	//Bite writter
	BitWritter w(data,2);
	
	//Set data
	w.Put(2,version);
	w.Put(1,padding);
	w.Put(1,extension);
	w.Put(4,csrcs.size());
	w.Put(1,mark);
	w.Put(7,payloadType);
	
	//set sequence number
	set2(data,2,sequenceNumber);
	
	//Get sequence number
	set4(data,4,timestamp);
	
	//Get ssrc
	set4(data,8,ssrc);
	
	int len=12;
	//Get all csrcs
	for (auto it=csrcs.begin();it!=csrcs.end();++it)
	{
		//Get them
		set4(data,len,*it);
		//Increas len
		len += 4;
	}
	
	//Return size
	return len;
	
}
DWORD RTPHeader::GetSize() const
{
	return 12+csrcs.size()*4;
}

void RTPHeader::Dump() const
{
	Debug("[RTPHeader v=%d p=%d x=%d cc=%d m=%d pt=%d seq=%u ts=%u ssrc=%u/]\n",
		version,
		padding,
		extension,
		csrcs.size(),
		mark,
		payloadType,
		sequenceNumber,
		timestamp,
		ssrc
	);
}