#include "rtp/RTPPayload.h"

RTPPayload::RTPPayload()
{
	//Reset payload
	payload  = buffer.data() + PREFIX;
	payloadLen = 0;
}

RTPPayload::shared RTPPayload::Clone()
{
	//New one
	auto cloned = std::make_shared<RTPPayload>();
	//Copy buffer data
	memcpy(cloned->buffer.data(),buffer.data(),buffer.size());
	//Reset payload pointers
	cloned->payload = cloned->buffer.data() + (payload-buffer.data());
	cloned->payloadLen = payloadLen;
	//Return it
	return cloned;
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
