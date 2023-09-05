#include "log.h"
#include "tools.h"
#include "rtmp/rtmpchunk.h"
#include <stdexcept>
#include <cstdlib>

/**************************************
 * RTMPChunkStreamInfo
 * 	Base information needed to process chunks in a chunk stream
 **************************************/
RTMPChunkStreamInfo::RTMPChunkStreamInfo()
{
	type = RTMPMessage::Type(0);
	timestamp = 0;
	timestampDelta = 0;
	streamId = -1;
	length = -1;
	isExtendedTimestamp = false;
}	

void RTMPChunkStreamInfo::SetTimestamp(DWORD ts)
{
	//Set timestamp
	timestamp = ts;
}
void RTMPChunkStreamInfo::SetTimestampDelta(DWORD delta)
{
	//Set delta
	timestampDelta = delta;
}

DWORD RTMPChunkStreamInfo::GetTimestamp()
{
	//Return timestamp
	return timestamp;
}


void RTMPChunkStreamInfo::IncreaseTimestampWithDelta()
{
	//Increase timestamp with delta value
	timestamp += timestampDelta;
}

void RTMPChunkStreamInfo::SetMessageTypeId(BYTE type)
{
	//Store value
	this->type = (RTMPMessage::Type)type;
}

void RTMPChunkStreamInfo::SetMessageStreamId(DWORD streamId)
{
	//Store value
	this->streamId = streamId;
}

void RTMPChunkStreamInfo::SetMessageLength(DWORD length)
{
	//Store value
	this->length = length;
}

DWORD RTMPChunkStreamInfo::GetMessageStreamId()
{
	return streamId;
}

BYTE RTMPChunkStreamInfo::GetMessageTypeId()
{
	return (BYTE)type;
}

void RTMPChunkStreamInfo::SetIsExtendedTimestamp(bool isExtended)
{
	isExtendedTimestamp = isExtended;
}

bool RTMPChunkStreamInfo::IsExtendedTimestamp() const
{
	return isExtendedTimestamp;
}

/***********************************
 * RTMPChunkInputStream
 * 	Chunk stream sent by the remote peer
 ***********************************/
RTMPChunkInputStream::RTMPChunkInputStream() : RTMPChunkStreamInfo()
{
	message = NULL;
}

RTMPChunkInputStream::~RTMPChunkInputStream()
{
	if (message)
		delete (message);
}

void RTMPChunkInputStream::StartChunkData()
{
	//If we don't have an message object
	if (!message)
		//Create a new one
		message = new RTMPMessage(streamId,timestamp,type,length);
}

DWORD RTMPChunkInputStream::Parse(BYTE *data,DWORD size)
{
	return message->Parse(data,size);
}

bool RTMPChunkInputStream::IsParsed()
{
	return message?message->IsParsed():false;
}
RTMPMessage* RTMPChunkInputStream::GetMessage()
{
	RTMPMessage *ret = NULL;
	//Check if it is parsed
	if (message->IsParsed())
	{
		//Return the parsed message
		ret = message;
		//Nullify
		message = NULL;
	}
	//Exit
	return ret;
}
bool RTMPChunkInputStream::IsFirstChunk()
{
	return message==NULL;
}

/***********************************
 * RTMPChunkOutputStream
 * 	Chunk stream sent by the local server
 ***********************************/
RTMPChunkOutputStream::RTMPChunkOutputStream(DWORD chunkStreamId) : RTMPChunkStreamInfo()
{
	//Empty message
	message = NULL;
	msgBuffer = NULL;
	//Store own id
	this->chunkStreamId = chunkStreamId;
	//Init mutex
	pthread_mutex_init(&mutex,0);
}

RTMPChunkOutputStream::~RTMPChunkOutputStream()
{
	//lock now
	pthread_mutex_lock(&mutex);
	//Clean messages in queue
	for (RTMPMessages::iterator it=messages.begin(); it!=messages.end(); ++it)
		//Delete message	
		delete(*it);

	if (message)
	{
		delete(msgBuffer);
		delete(message);
	}
	//Unlock
	pthread_mutex_unlock(&mutex);
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

void RTMPChunkOutputStream::SendMessage(RTMPMessage *msg)
{
	//lock now
	pthread_mutex_lock(&mutex);
	//Check it is not null
	if(msg)
		//Push back the message
		messages.push_back(msg);
	//Unlock
	pthread_mutex_unlock(&mutex);
}

DWORD RTMPChunkOutputStream::GetNextChunk(BYTE *data,DWORD size,DWORD maxChunkSize)
{
	//lock now
	pthread_mutex_lock(&mutex);
	//Message basic header
	RTMPChunkBasicHeader header;
	//Set chunk stream id
	header.SetStreamId(chunkStreamId);
	//Chunk header
	RTMPObject* chunkHeader = NULL;
	//Extended timestamp
	RTMPExtendedTimestamp extts;
	//Use extended timestamp flag
	bool useExtTimestamp = false;

	//If we are not processing an object
	if (!message)
	{
		//Check we hve still data
		if (messages.empty())
		{
			//Unlock
			pthread_mutex_unlock(&mutex);
			//No more data to send here
			return 0;
		}
		//Get the next message to send
		message = messages.front();
		//Remove from queue
		messages.pop_front();
		//Get message values
		RTMPMessage::Type    msgType	= message->GetType();
		DWORD   msgStreamId 		= message->GetStreamId();
		DWORD   msgLength 		= message->GetLength();
		DWORD   msgTimestamp 		= message->GetTimestamp();
		DWORD	msgTimestampDelta 	= msgTimestamp-timestamp;
		//Start sending 
		pos = 0;

		//Allocate data for serialized message
		msgBuffer = (BYTE*)malloc(msgLength);
		//Serialize it
		message->Serialize(msgBuffer,msgLength);

		//Select wich header
		if (!msgStreamId || msgStreamId!=streamId || msgTimestamp<timestamp)
		{
			//Create chunk header type 0 (last check is for backward time on Seek)
			RTMPChunkType0* type0 = new RTMPChunkType0();
			//Set header type
			header.SetFmt(0);
			//Check timestamp
			if (msgTimestamp>=0xFFFFFF)
			{
				//Set flag
				useExtTimestamp = true;
				//Use extended header
				type0->SetTimestamp(0xFFFFFF);
				//Set it
				extts.SetTimestamp(msgTimestamp);

			} else {
				//Set timestamp
				type0->SetTimestamp(msgTimestamp);
			}
			//Set data in chunk header
			type0->SetMessageLength(msgLength);
			type0->SetMessageTypeId(msgType);
			type0->SetMessageStreamId(msgStreamId);
			//Not delta available for next packet
			msgTimestampDelta = 0;
			//Store object
			chunkHeader = type0;
		} else if (msgLength!=length || msgType!=type) {
			//Create chunk header type 1
			RTMPChunkType1* type1 = new RTMPChunkType1();
			//Set header type
			header.SetFmt(1);
			//Set data in chunk header
			type1->SetTimestampDelta(msgTimestampDelta);
			type1->SetMessageLength(msgLength);
			type1->SetMessageTypeId(msgType);
			//Store object
			chunkHeader = type1;
		} else if (msgTimestampDelta!=timestampDelta) {
			//Create chunk header type 1
			RTMPChunkType2* type2 = new RTMPChunkType2();
			//Set header type
			header.SetFmt(2);
			//Set data in chunk header
			type2->SetTimestampDelta(msgTimestampDelta);
			//Store object
			chunkHeader = type2;
		} else {
			//Set header type 3 as it shares all data with previous
			header.SetFmt(3);
			//Store object
			chunkHeader = NULL;
		}
		//And update the stream values with latest message values
		SetTimestamp(msgTimestamp);
		SetTimestampDelta(msgTimestampDelta);
		SetMessageLength(msgLength);
		SetMessageTypeId(msgType);
		SetMessageStreamId(msgStreamId);
	} else {
		//Set header type 3 as it shares all data with previous
		header.SetFmt(3);
		//Store object
		chunkHeader = NULL;
	}

	//Serialize header
	DWORD headersLen = header.Serialize(data,size);
	//Check if we need chunk header
	if (chunkHeader)
		//Serialize chunk header
		headersLen += chunkHeader->Serialize(data+headersLen,size-headersLen);
	//Check if need to use extended timestamp
	if (useExtTimestamp)
		//Serialize extened header
		headersLen += extts.Serialize(data+headersLen,size-headersLen);

	//Size of the msg data of the chunk
	DWORD payloadLen = maxChunkSize;
	//If we have more than needed
	if (payloadLen>length-pos)
		//Just copy until the oend of the object
		payloadLen = length-pos;
	
	//Copy
	memcpy(data+headersLen,msgBuffer+pos,payloadLen);

	//Increase sent data from msg
	pos += payloadLen;
	//Check if we have finished with this message	
	if (pos==length)
	{
		//Delete buffer
		free(msgBuffer);
		//Null
		msgBuffer = NULL;
		//Delete message
		delete(message);
		//Next one
		message = NULL;
	}

	//Check
	if (chunkHeader)
		//Delete it
		delete (chunkHeader);

	//Unlock
	pthread_mutex_unlock(&mutex);

	//Return copied data
	return headersLen+payloadLen;	
}

bool RTMPChunkOutputStream::HasData()
{
	//lock now
	pthread_mutex_lock(&mutex);
	//Return true if we are sending a message or we have more in the queue
	bool ret =  message || !messages.empty();
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	return ret;
}

bool RTMPChunkOutputStream::ResetStream(DWORD id)
{
	Log("-ResetStream %d\n",id);
	
	bool abort = false;
	
	//lock now
	pthread_mutex_lock(&mutex);
	//Iterate the messages
	RTMPMessages::iterator it = messages.begin();
	//remove any message from the stream
	while(it!=messages.end())
	{
		//Get Message
		RTMPMessage *msg = *it;
		//Get message
		if (msg && msg->GetStreamId()==id)
			//Remove it
			it = messages.erase(it);
		else
			//next one;
			 ++it;
	}

	//If we have message of this stream
	if (message && message->GetStreamId()==id)
	{
		//Delete buffer
		free(msgBuffer);
		//Null
		msgBuffer = NULL;
		//Delete message
		delete(message);
		//Next one
		message = NULL;
		//We have to abort
		abort = true;
	}
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//NOt needed to abort
	return abort;
}
