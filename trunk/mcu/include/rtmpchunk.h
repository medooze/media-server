#ifndef _RTMP_CHUNK_H_
#define _RTMP_CHUNK_H_
#include "config.h"
#include "rtmp.h"
#include "rtmpmessage.h"
#include <list>

class RTMPChunkStreamInfo
{
public:
	RTMPChunkStreamInfo();

	void SetTimestamp(QWORD timestamp);
	void SetTimestampDelta(QWORD delta);
	void IncreaseTimestampWithDelta();
	void SetMessageLength(DWORD length);
	void SetMessageTypeId(BYTE type);
	void SetMessageStreamId(DWORD id);

	DWORD GetMessageStreamId();
	BYTE  GetMessageTypeId();
	QWORD GetTimestamp();

protected:
	RTMPMessage::Type	type;
	DWORD 			streamId;
	DWORD			length;
	QWORD			timestamp;
	QWORD			timestampDelta;
};


class RTMPChunkOutputStream : public RTMPChunkStreamInfo
{
public:
	RTMPChunkOutputStream(DWORD id);
	~RTMPChunkOutputStream();
	void SendMessage(RTMPMessage* msg);
	bool HasData();
	bool ResetStream(DWORD id);
	DWORD GetNextChunk(BYTE *data,DWORD size,DWORD maxChunkSize);

private:
	typedef std::list<RTMPMessage*> RTMPMessages;
private:
	RTMPMessages messages;	
	DWORD chunkStreamId;
	RTMPMessage* message;
	DWORD pos;
	BYTE* msgBuffer;
	pthread_mutex_t mutex;
};

class RTMPChunkInputStream : public RTMPChunkStreamInfo
{
public:
	RTMPChunkInputStream();
	~RTMPChunkInputStream();
	void StartChunkData();
	DWORD Parse(BYTE* data,DWORD size);
	bool IsParsed();
	RTMPMessage* GetMessage();
	bool IsFirstChunk();
private:
	RTMPMessage* message;
};

#endif
