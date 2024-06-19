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
#include "TlsClient.h"

#include <pthread.h>
#include <map>

class RTMPClientConnection :
	public RTMPMediaStream::Listener
{
public:

	enum class ErrorCode
	{
		NoError = 0,
		Generic = 1,
		FailedToResolveURL = 2,
		GetSockOptError = 3,
		FailedToConnectSocket = 4,
		ConnectCommandFailed = 5,
		FailedToParseData = 6,
		PeerClosed = 7,
		ReadError = 8,
		PollError = 9
	};

	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener() {};
	public:
		//Interface
		virtual void onConnected(RTMPClientConnection* conn) = 0;
		virtual void onDisconnected(RTMPClientConnection* conn, ErrorCode code) = 0;
		virtual void onCommand(RTMPClientConnection* conn, DWORD messageStreamId, const wchar_t* name, AMFData* obj, const std::vector<AMFData*>&) = 0;
	};
public:
	RTMPClientConnection(bool secure, const std::wstring& tag);
	virtual ~RTMPClientConnection();

	ErrorCode Connect(const char* server, int port, const char* app, RTMPClientConnection::Listener* listener);
	DWORD SendCommand(DWORD streamId, const wchar_t* name, AMFData* params, AMFData* extra, std::function<void(bool, AMFData*, const std::vector<AMFData*>&)> callback);
	DWORD SendCommand(DWORD streamId, const wchar_t* name, AMFData* params, AMFData* extra);
	int Disconnect();

	void  SetUserData(DWORD data) { this->data = data; }
	DWORD GetUserData() { return data; }

	//Listener for the media data
	virtual void onAttached(RTMPMediaStream* stream);
	virtual void onMediaFrame(DWORD id, RTMPMediaFrame* frame);
	virtual void onMetaData(DWORD id, RTMPMetaData* meta);
	virtual void onCommand(DWORD id, const wchar_t* name, AMFData* obj);
	virtual void onStreamBegin(DWORD id);
	virtual void onStreamEnd(DWORD id);
	virtual void onStreamReset(DWORD id);
	virtual void onDetached(RTMPMediaStream* stream);


	QWORD GetInBytes() const	{ return inBytes;	}
	QWORD GetOutBytes() const	{ return outBytes;	}

protected:
	void Start();
	void Stop();
	int Run();
private:
	
	void sendRtmpData(const uint8_t* data, size_t size);
	void processReceivedData(const uint8_t* data, size_t size);

	static  void* run(void* par);
	void ParseData(const BYTE* data, const DWORD size);
	DWORD SerializeChunkData(BYTE* data, const DWORD size);
	int WriteData(const BYTE* data, const DWORD size);

	void ProcessControlMessage(DWORD streamId, BYTE type, RTMPObject* msg);
	void ProcessCommandMessage(DWORD streamId, RTMPCommandMessage* cmd);
	void ProcessMediaData(DWORD streamId, RTMPMediaFrame* frame);
	void ProcessMetaData(DWORD streamId, RTMPMetaData* frame);

	void SendCommandError(DWORD streamId, QWORD transId, AMFData* params = NULL, AMFData* extra = NULL);
	void SendCommandResult(DWORD streamId, QWORD transId, AMFData* params, AMFData* extra);
	void SendCommandResponse(DWORD streamId, const wchar_t* name, QWORD transId, AMFData* params, AMFData* extra);


	void SendControlMessage(RTMPMessage::Type type, RTMPObject* msg);

	void SignalWriteNeeded();
private:

	enum State { NONE = 0, HEADER_S0_WAIT = 1, HEADER_S1_WAIT = 2, HEADER_S2_WAIT = 3, CHUNK_HEADER_WAIT = 4, CHUNK_TYPE_WAIT = 5, CHUNK_EXT_TIMESTAMP_WAIT = 6, CHUNK_DATA_WAIT = 7 };

	typedef std::map<DWORD, RTMPChunkInputStream*>  RTMPChunkInputStreams;
	typedef std::map<DWORD, RTMPChunkOutputStream*> RTMPChunkOutputStreams;
	typedef std::map<DWORD, std::function<void(bool, AMFData*, const std::vector<AMFData*>&)>> Transactions;
private:
	int fd = FD_INVALID;
	pollfd ufds[1] = {};
	bool inited = false;
	bool running = false;
	State state = State::NONE;

	RTMPHandshake01 c01;
	RTMPHandshake2 c2;
	RTMPHandshake0 s0;
	RTMPHandshake1 s1;
	RTMPHandshake2 s2;

	bool digest = false;

	DWORD videoCodecs = 0;
	DWORD audioCodecs = 0;

	RTMPChunkBasicHeader header;
	RTMPChunkType0	type0;
	RTMPChunkType1	type1;
	RTMPChunkType2	type2;
	RTMPExtendedTimestamp extts;

	RTMPChunkInputStreams  	chunkInputStreams;
	RTMPChunkOutputStreams  chunkOutputStreams;
	RTMPChunkInputStream*	chunkInputStream = nullptr;
	Transactions		transactions;

	DWORD chunkStreamId = 0;
	DWORD chunkLen = 0;
	DWORD maxChunkSize = 128;
	DWORD maxOutChunkSize = 512;

	pthread_t thread;
	pthread_mutex_t mutex;

	std::wstring	 appName;
	std::wstring	 tcUrl;
	DWORD maxStreamId = -1;
	DWORD maxTransId = 0;

	Listener* listener = nullptr;

	timeval startTime;
	DWORD windowSize = 0;
	DWORD curWindowSize = 0;
	DWORD recvSize = 0;
	QWORD inBytes = 0;
	QWORD outBytes = 0;

	bool	needsAuth = false;
	DWORD	data = 0;
	std::wstring tag;
	std::wstring user;
	std::wstring pwd;
	std::wstring method;
	std::wstring challenge;
	std::wstring opaque;
	
	std::unique_ptr<TlsClient> tls;
};

#endif
