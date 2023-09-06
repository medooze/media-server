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

	void SetTimestamp(DWORD timestamp);
	void SetTimestampDelta(DWORD delta);
	void IncreaseTimestampWithDelta();
	void SetMessageLength(DWORD length);
	void SetMessageTypeId(BYTE type);
	void SetMessageStreamId(DWORD id);

	DWORD GetMessageStreamId();
	BYTE  GetMessageTypeId();
	DWORD GetTimestamp();

	void SetIsExtendedTimestamp(bool isExtended);
	bool IsExtendedTimestamp() const;
protected:
	RTMPMessage::Type	type;
	DWORD 			streamId;
	DWORD			length;
	DWORD			timestamp;
	DWORD			timestampDelta;
	bool 			isExtendedTimestamp;
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
	DWORD chunkStreamId = 0;
	RTMPMessage* message = nullptr;
	DWORD pos = 0;
	BYTE* msgBuffer = nullptr;
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
