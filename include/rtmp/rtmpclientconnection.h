#ifndef _RTMCLIENTPCONNECTION_H_
#define _RTMCLIENTPCONNECTION_H_
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

class RTMPClientConnection :
	public RTMPMediaStream::Listener
{
public:
	class NetStream :
		public RTMPPipedMediaStream
	{
	public:
		class Listener
		{
		public:
			//Virtual desctructor
			virtual ~Listener(){};
		public:
			//Interface
			virtual void onNetStreamStatus(RTMPClientConnection *conn,NetStream *sream,const RTMPNetStatusEventInfo &info,const wchar_t* message) = 0;
		};
	public:
		NetStream(DWORD id,RTMPClientConnection *conn);
		~NetStream();

		//Netstream API
		bool Play(std::wstring& url);
		bool Seek(DWORD time);
		bool Pause();
		bool Resume();
		bool Close();
		bool Publish(std::wstring& url);
		bool UnPublish();

	protected:
		virtual void fireOnNetStreamStatus(QWORD transId,const RTMPNetStatusEventInfo &info,const wchar_t* message);

	protected:
		friend class RTMPClientConnection;
		RTMPClientConnection* conn;
	};

	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onConnected(RTMPClientConnection* conn) = 0;
		virtual void onNetStreamCreated(RTMPClientConnection* conn,NetStream *stream) = 0;
		virtual void onCommandResponse(RTMPClientConnection* conn,DWORD id,bool isError,AMFData* param) = 0;
		virtual void onDisconnected(RTMPClientConnection* conn) = 0;
	};
public:
	RTMPClientConnection(const std::wstring& tag);
	virtual ~RTMPClientConnection();

	int Connect(const char* server,int port, const char* app,RTMPClientConnection::Listener *listener);
	DWORD CreateStream(const std::wstring &tag);
	DWORD Call(const wchar_t* name,AMFData* params,AMFData *extra);
	void DeleteStream(RTMPMediaStream *stream);
	int Disconnect();

	void  SetUserData(DWORD data)	{ this->data = data;	}
	DWORD GetUserData()		{ return data;		}

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
private:
	friend class NetStream;

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
	
	DWORD SendCommand(DWORD messageStreamId,const wchar_t* name,AMFData* params,AMFData *extra);
	void SendControlMessage(RTMPMessage::Type type,RTMPObject* msg);
	
	void SignalWriteNeeded();
private:

	enum State {NONE=0,HEADER_S0_WAIT=1,HEADER_S1_WAIT=2,HEADER_S2_WAIT=3,CHUNK_HEADER_WAIT=4,CHUNK_TYPE_WAIT=5,CHUNK_EXT_TIMESTAMP_WAIT=6,CHUNK_DATA_WAIT=7};
	enum TransType {CALL,CREATESTREAM};

	struct TransInfo
	{
		TransInfo()
		{
			
		}
		TransInfo(TransType type)
		{
			//Store values
			this->type = type;
			this->par = 0;
		}

		TransInfo(TransType type,DWORD par)
		{
			//Store values
			this->type = type;
			this->par = par;
		}
		
		TransInfo(TransType type,DWORD par,const wchar_t *tag)
		{
			//Store values
			this->type = type;
			this->par = par;
			this->tag.assign(tag);
		}

		TransInfo(TransType type,DWORD par,const std::wstring& tag)
		{
			//Store values
			this->type = type;
			this->par = par;
			this->tag = tag;
		}
		
		TransType type {};
		DWORD par = 0;
		std::wstring tag;

	};
	typedef std::map<DWORD,RTMPChunkInputStream*>  RTMPChunkInputStreams;
	typedef std::map<DWORD,RTMPChunkOutputStream*> RTMPChunkOutputStreams;
	typedef std::map<DWORD,NetStream*> RTMPNetStreams;
	typedef std::map<DWORD,TransInfo> Transactions;
private:
	int fd;
	pollfd ufds[1] = {};
	bool inited;
	bool running;
	State state;

	RTMPHandshake01 c01;
	RTMPHandshake2 c2;
	RTMPHandshake0 s0;
	RTMPHandshake1 s1;
	RTMPHandshake2 s2;
	
	bool digest;

	DWORD videoCodecs = 0;
	DWORD audioCodecs = 0;

	RTMPChunkBasicHeader header;
	RTMPChunkType0	type0;
	RTMPChunkType1	type1;
	RTMPChunkType2	type2;
	RTMPExtendedTimestamp extts;

	RTMPChunkInputStreams  	chunkInputStreams;
	RTMPChunkOutputStreams  chunkOutputStreams;
	RTMPChunkInputStream* 	chunkInputStream = nullptr;
	Transactions		transactions;

	DWORD chunkStreamId = 0;
	DWORD chunkLen = 0;
	DWORD maxChunkSize;
	DWORD maxOutChunkSize;

	pthread_t thread;
	pthread_mutex_t mutex;

	std::wstring	 appName;
	std::wstring	 tcUrl;
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

	bool	needsAuth = false;
	DWORD	data = 0;
	std::wstring tag;
	std::wstring user;
	std::wstring pwd;
	std::wstring method;
	std::wstring challenge;
	std::wstring opaque;
};

#endif
