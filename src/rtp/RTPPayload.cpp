#include "rtp/RTPPayload.h"

RTPPayload::RTPPayload()
{
	//Reset payload
	payload  = buffer.data() + PREFIX;
	payloadLen = 0;
}

void RTPPayload::Reset()
{
	//Reset payload
	payload = buffer.data() + PREFIX;
	payloadLen = 0;
}

bool RTPPayload::SetPayload(const BYTE *data,DWORD size)
{
	//Check size
	if (size>GetMaxMediaLength())
		//Error
		return false;
	//Reset payload
	payload  = buffer.data() + PREFIX;
	//Copy
	memcpy(payload,data,size);
	//Set length
	payloadLen = size;
	//good
	return true;
}

bool RTPPayload::SetPayload(const RTPPayload& other)
{
	//Copy buffer data
	memcpy(buffer.data(), other.buffer.data(), other.buffer.size());
	//Reset payload pointers
	payload = buffer.data() + (other.payload - other.buffer.data());
	payloadLen = other.payloadLen;
	//good
	return true;
}

bool RTPPayload::PrefixPayload(BYTE *data,DWORD size)
{
	//Check size
	if (size>payload-buffer.data())
		//Error
		return false;
	//Copy
	memcpy(payload-size,data,size);
	//Set pointers
	payload  -= size;
	payloadLen += size;
	//good
	return true;
}


bool RTPPayload::SkipPayload(DWORD skip) 
{
	//Ensure we have enough to skip
	if (GetMaxMediaLength()<skip+(payload-buffer.data())+payloadLen)
		//Error
		return false;

	//Move media data
	payload += skip;
	//Set length
	payloadLen -= skip;
	//good
	return true;
}
