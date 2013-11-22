#ifndef _RTMPCONNECTION_H_
#define _RTMPCONNECTION_H_
#include <pthread.h>
#include <sys/poll.h>
#include "config.h"
#include "rtmp.h"
#include "rtmpchunk.h"
#include "rtmpmessage.h"
#include "rtmpstream.h"
#include "rtmpapplication.h"
#include <pthread.h>
#include <map>


class RTMPConnection :
	public RTMPNetConnection::Listener,
	public RTMPMediaStream::Listener,
	public RTMPNetStream::Listener
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual RTMPNetConnection* OnConnect(const std::wstring& appName,RTMPNetConnection::Listener *listener) = 0;
		virtual void onDisconnect(RTMPConnection *con) = 0;
	};
public:
	RTMPConnection(Listener* listener);
	~RTMPConnection();

	int Init(int fd);
	int End();

	//Listener from NetConnection
	virtual void onNetConnectionStatus(const RTMPNetStatusEventInfo &info,const wchar_t *message);
	virtual void onNetConnectionDisconnected();
	//Listener from NetStream
	virtual void onNetStreamDestroyed(DWORD id);
	virtual void onNetStreamStatus(DWORD id,const RTMPNetStatusEventInfo &nfo,const wchar_t *message);
	//Listener for the media data
	virtual void onAttached(RTMPMediaStream *stream);
	virtual void onMediaFrame(DWORD id,RTMPMediaFrame *frame);
	virtual void onMetaData(DWORD id,RTMPMetaData *meta);
	virtual void onCommand(DWORD id,const wchar_t *name,AMFData* obj);
	virtual void onStreamBegin(DWORD id);
	virtual void onStreamEnd(DWORD id);
	//virtual void onStreamIsRecorded(DWORD id);
	virtual void onStreamReset(DWORD id);
	virtual void onDetached(RTMPMediaStream *stream);
	
protected:
	void Start();
	void Stop();
	int Run();
	void PingRequest();
private:
	static  void* run(void *par);
	void ParseData(BYTE *data,const DWORD size);
	DWORD SerializeChunkData(BYTE *data,const DWORD size);
	int WriteData(BYTE *data,const DWORD size);

	void ProcessControlMessage(DWORD messageStremId,BYTE type,RTMPObject* msg);
	void ProcessCommandMessage(DWORD messageStremId,RTMPCommandMessage* cmd);
	void ProcessMediaData(DWORD messageStremId,RTMPMediaFrame* frame);
	void ProcessMetaData(DWORD messageStremId,RTMPMetaData* frame);

	void SendCommandError(DWORD messageStreamId,QWORD transId,AMFData* params = NULL,AMFData *extra = NULL);
	void SendCommandResult(DWORD messageStreamId,QWORD transId,AMFData* params,AMFData *extra);
	void SendCommandResponse(DWORD messageStreamId,const wchar_t* name,QWORD transId,AMFData* params,AMFData *extra);
	
	void SendCommand(DWORD messageStreamId,const wchar_t* name,AMFData* params,AMFData *extra);
	void SendControlMessage(RTMPMessage::Type type,RTMPObject* msg);
	
	void SignalWriteNeeded();
private:
	enum State {HEADER_C0_WAIT=0,HEADER_C1_WAIT=1,HEADER_C2_WAIT=2,CHUNK_HEADER_WAIT=3,CHUNK_TYPE_WAIT=4,CHUNK_EXT_TIMESTAMP_WAIT=5,CHUNK_DATA_WAIT=6};
	typedef std::map<DWORD,RTMPChunkInputStream*>  RTMPChunkInputStreams;
	typedef std::map<DWORD,RTMPChunkOutputStream*> RTMPChunkOutputStreams;
	typedef std::map<DWORD,RTMPNetStream*> RTMPNetStreams;
private:
	int socket;
	pollfd ufds[1];
	bool inited;
	bool running;
	State state;

	RTMPHandshake01 s01;
	RTMPHandshake0 c0;
	RTMPHandshake1 c1;
	RTMPHandshake2 s2;
	RTMPHandshake2 c2;
	
	bool digest;

	DWORD videoCodecs;
	DWORD audioCodecs;

	RTMPChunkBasicHeader header;
	RTMPChunkType0	type0;
	RTMPChunkType1	type1;
	RTMPChunkType2	type2;
	RTMPExtendedTimestamp extts;

	RTMPChunkInputStreams  	chunkInputStreams;
	RTMPChunkOutputStreams  chunkOutputStreams;
	RTMPChunkInputStream* 	chunkInputStream;

	DWORD chunkStreamId;
	DWORD chunkLen;
	DWORD maxChunkSize;
	DWORD maxOutChunkSize;

	pthread_t thread;
	pthread_mutex_t mutex;

	RTMPNetConnection* app;
	std::wstring	 appName;
	RTMPNetStreams	 streams;
	DWORD maxStreamId;
	DWORD maxTransId;

	Listener* listener;

	timeval startTime;
	DWORD windowSize;
	DWORD curWindowSize;
	DWORD recvSize;
	DWORD inBytes;
	DWORD outBytes;

	QWORD bandIni;
	DWORD bandSize;
	DWORD bandCalc;
};

#endif
