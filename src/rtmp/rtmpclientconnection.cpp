#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <vector>
#include "log.h"
#include "assertions.h"
#include "tools.h"
#include "rtmp/rtmphandshake.h"
#include "rtmp/rtmpclientconnection.h"




/********************************
 * RTMP connection demultiplex buffers streams from incoming raw data
 * extracting the individual buffers and passes the message fragments
 * to the message layer.
 *******************************************************************/

RTMPClientConnection::RTMPClientConnection(const std::wstring& tag) :
	tag(tag)
{
	setZeroThread(&thread);
	//Set initial time
	gettimeofday(&startTime, 0);
	//Init mutex
	pthread_mutex_init(&mutex, 0);
	//Create output chunk streams for control
	chunkOutputStreams[2] = new RTMPChunkOutputStream(2);
	//Create output chunk streams for command
	chunkOutputStreams[3] = new RTMPChunkOutputStream(3);
	//Create output chunk streams for audio
	chunkOutputStreams[4] = new RTMPChunkOutputStream(4);
	//Create output chunk streams for video
	chunkOutputStreams[5] = new RTMPChunkOutputStream(5);
}

RTMPClientConnection::~RTMPClientConnection()
{
	//End just in case
	Disconnect();
	//TODO: Clean all!!
	delete(chunkOutputStreams[2]);
	delete(chunkOutputStreams[3]);
	delete(chunkOutputStreams[4]);
	delete(chunkOutputStreams[5]);
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

int RTMPClientConnection::Connect(const char* server, int port, const char* app, Listener* listener)
{
	sockaddr_in addr;
	hostent* host;

	Log(">RTMPClientConnection::Connect() [host:%s:%d,url:%s]\n", server, port, app);

	//Create socket
	fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	//Set no delay option
	int flag = 1;
// Ignore coverity error: "this->fd" is passed to a parameter that cannot be negative.
// coverity[negative_returns]
	(void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
	//Catch all IO errors
	signal(SIGIO, EmptyCatch);

	//Get ip of server
	host = gethostbyname(server);

	//If not found
	if (!host)
	{
		//Error
		if (listener)
		{
			listener->onError(this, RTMPClientConnection::ErrorCode::FailedToResolveURL);
		}
			
		return Error("-RTMPClientConnection::Connect() Could not resolve %s\n", server);
	}

	//Set to zero
	bzero((char*)&addr, sizeof(addr));

	//Set properties
	addr.sin_family = AF_INET;
	memcpy((char*)&addr.sin_addr.s_addr, host->h_addr, host->h_length);
	addr.sin_port = htons(port);

	//Connect
// Ignore coverity error: "this->fd" is passed to a parameter that cannot be negative.
// coverity[negative_returns]
	if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0 && errno != EINPROGRESS)
	{
		if (listener)
		{
			listener->onError(this, RTMPClientConnection::ErrorCode::FailedToConnectSocket);
		}
		
		//Exit
		return Error("Connection error [%d]\n", errno);
	}

	//I am inited
	inited = true;

	wchar_t aux[2048];
	//Convert the app name
	swprintf(aux, 2048, L"%s", app);
	//Store app name
	appName.assign(aux);

	//Create url
	swprintf(aux, 2048, L"rtmp://%s:%d/%s", server, port, app);

	//Set it
	tcUrl.assign(aux);

	//Store listener
	this->listener = listener;

	//Start
	Start();

	return 1;
}

void RTMPClientConnection::Start()
{
	//We are running
	running = true;

	//Create thread
	createPriorityThread(&thread, run, this, 0);
}

void RTMPClientConnection::Stop()
{
	//If got socket
	if (fd != FD_INVALID)
	{
		//Not running;
		running = false;
		//Close socket
		shutdown(fd, SHUT_RDWR);
		//Will cause poll to return
		MCU_CLOSE(fd);
		//No socket
		fd = FD_INVALID;
	}
}

int RTMPClientConnection::Disconnect()
{
	//Check we have been inited
	if (!inited)
		//Exit
		return 0;

	Log(">RTMPClientConnection::Disconnect() Ending  RTMP connection\n");

	//Not inited any more
	inited = false;

	//Stop just in case
	Stop();

	//If running
	if (!isZeroThread(thread))
	{
		//Wait for server thread to close
		pthread_join(thread, NULL);
		//No thread
		setZeroThread(&thread);
	}

	listener = NULL;
	
	//Ended
	Log("<RTMPClientConnection::Disconnect() Ended RTMP connection\n");

	return 1;
}

/***********************
* run
*       Helper thread function
************************/
void* RTMPClientConnection::run(void* par)
{
	//Block signals to avoid exiting on SIGUSR1
	blocksignals();

	//Obtenemos el parametro
	RTMPClientConnection* con = (RTMPClientConnection*)par;

	//Ejecutamos
	con->Run();
	//Exit
	return NULL;
}

/***************************
 * Run
 * 	Server running thread
 ***************************/
int RTMPClientConnection::Run()
{
	BYTE data[4096*2];
	unsigned int size = 4096*2;
	bool connected = false;

	Log("-RTMPClientConnection::Run() connection [%p]\n", this);

	//Set values for polling
	ufds[0].fd = fd;
	ufds[0].events = POLLOUT | POLLIN | POLLERR | POLLHUP;

	//Run until ended
	while (running)
	{
		//Wait for events
		if (poll(ufds, 1, -1) < 0)
			//Check again
			continue;

		if (ufds[0].revents & POLLOUT)
		{
			//Check if socket was not connected yet
			if (!connected)
			{
				//Double check it is connected
				int err;
				socklen_t len = sizeof(err);
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1)
				{
					Warning("-RTMPClientConnection::Run() getsockopt failed [%p]\n", this);
					//Disconnect application
					if (listener) listener->onError(this, RTMPClientConnection::ErrorCode::Generic);
					//exit
					break;
				}

				//Check status
				if (err != 0)
				{
					Warning("-RTMPClientConnection::Run() getsockopt error [%p, errno:%d]\n", this, err);
					//Disconnect application
					if (listener) listener->onError(this, RTMPClientConnection::ErrorCode::Generic);
					//exit
					break;
				}

				//Connected
				connected = true;

				//Create C01 and send it
				c01.SetRTMPVersion(3);
				c01.SetTime(getDifTime(&startTime) / 1000);
				c01.SetVersion(0, 0, 0, 0);
				//Do not calculate digest
				digest = false;
				//Set state
				state = HEADER_S0_WAIT;
				//Send it
				WriteData(c01.GetData(), c01.GetSize());

				//Debug
				Debug("-RTMPClientConnection::Run() Socket connected, Sending c0 and c1 with digest %s size %d\n", digest ? "on" : "off", c01.GetSize());
			}

			//Write data buffer
			DWORD len = SerializeChunkData(data, size);
			//Send it
			WriteData(data, len);
			//Increase sent bytes
			outBytes += len;
		}

		if (ufds[0].revents & POLLIN)
		{
			//Read data from connection
			int len = read(fd, data, size);
			if (len == 0)
			{
				if (listener) listener->onError(this, RTMPClientConnection::ErrorCode::PeerClosed);
				break;
			}
			
			if (len < 0)
			{
				//Error
				Log("-RTMPClientConnection::Run() Readed [%d,%d]\n", len, errno);
				
				if (listener) listener->onError(this, RTMPClientConnection::ErrorCode::Generic);
				
				//Exit
				break;
			}
			//Increase in bytes
			inBytes += len;

			try {
				//Parse data
				ParseData(data, len);
			}
			catch (std::exception& e) {
				//Show error
				Error("-RTMPClientConnection::Run() Exception parsing data: %s\n", e.what());
				//Dump it
				Dump(data, len);
				
				if (listener) listener->onError(this, RTMPClientConnection::ErrorCode::FailedToParseData);
				
				//Break on any error
				break;
			}
		}

		if ((ufds[0].revents & POLLHUP) || (ufds[0].revents & POLLERR))
		{
			//Error
			Log("-RTMPClientConnection::Run() Pool error event [%d]\n", ufds[0].revents);
			
			if (listener) listener->onError(this, RTMPClientConnection::ErrorCode::Generic);
			
			//Exit
			break;
		}
	}
	
	//If got application
	if (listener)
	{
		//Disconnect application
		listener->onDisconnected(this);
	}
	
	Log("<RTMPClientConnection::Run() completed.\n");

	//Done
	return 1;
}

void RTMPClientConnection::SignalWriteNeeded()
{
	//lock now
	pthread_mutex_lock(&mutex);

	//Set to wait also for read events
	ufds[0].events = POLLIN | POLLOUT | POLLERR | POLLHUP;

	//Unlock
	pthread_mutex_unlock(&mutex);

	//Check thred
	if (!isZeroThread(thread))
		//Signal the pthread this will cause the poll call to exit
		pthread_kill(thread, SIGIO);
}

DWORD RTMPClientConnection::SerializeChunkData(BYTE* data, DWORD size)
{
	DWORD len = 0;

	//Lock mutex
	pthread_mutex_lock(&mutex);

	//Remove the write signal
	ufds[0].events = POLLIN | POLLERR | POLLHUP;

	//Iterate the chunks in ascendig order (more important firsts)
	for (RTMPChunkOutputStreams::iterator it = chunkOutputStreams.begin(); it != chunkOutputStreams.end(); ++it)
	{
		//Get stream
		RTMPChunkOutputStream* chunkOutputStream = it->second;

		//Check it it has data pending
		while (chunkOutputStream->HasData())
		{
			//Check if we do not have enought space left for more
			if (size - len < maxOutChunkSize + 12)
			{
				//We have more data to write
				ufds[0].events = POLLIN | POLLOUT | POLLERR | POLLHUP;
				//End this writing
				goto end;
			}

			//Write next chunk from this stream
			len += chunkOutputStream->GetNextChunk(data + len, size - len, maxOutChunkSize);

		}
	}
end:
	//Un Lock mutex
	pthread_mutex_unlock(&mutex);

	//Return chunks data length
	return len;
}

/***********************
 * ParseData
 * 	Process incomming data
 **********************/
void RTMPClientConnection::ParseData(BYTE* data, const DWORD size)
{
	RTMPChunkInputStreams::iterator it;
	int len = 0;
	int digesOffsetMethod = 0;

	//Get pointer and data size
	BYTE* buffer = data;
	DWORD bufferSize = size;
	DWORD digestPosServer = 0;

	//Increase current window
	curWindowSize += size;
	//And total size
	recvSize += size;

	//Check current window
	if (windowSize && curWindowSize > windowSize)
	{
		//Send
		SendControlMessage(RTMPMessage::Acknowledgement, RTMPAcknowledgement::Create(recvSize));
		//Reset window
		curWindowSize = 0;
	}

	//While there is data
	while (bufferSize > 0)
	{
		//Check connection state
		switch (state)
		{
			case HEADER_S0_WAIT:
				//Parse c0
				len = s0.Parse(buffer, bufferSize);
				//Move
				buffer += len;
				bufferSize -= len;
				//If it is parsed
				if (s0.IsParsed())
				{
					//Move to next state
					state = HEADER_S1_WAIT;
					//Debug
					Log("-RTMPClientConnection::ParseData() Received c0 version: %d\n", s0.GetRTMPVersion());
				}
				break;
			case HEADER_S1_WAIT:
				//Parse c1
				len = s1.Parse(buffer, bufferSize);
				//Move
				buffer += len;
				bufferSize -= len;
				//If it is parsed
				if (s1.IsParsed())
				{
					Log("-RTMPClientConnection::ParseData() Received S1 server version [%d,%d,%d,%d]\n", s1.GetVersion()[0], s1.GetVersion()[1], s1.GetVersion()[2], s1.GetVersion()[3]);
					//Set s2 data
					c2.SetTime(s1.GetTime());
					//Set current timestamp
					c2.SetTime2(getDifTime(&startTime) / 1000);
					//Echo c1 data
					c2.SetRandom(s1.GetRandom(), s1.GetRandomSize());
					//Move to next state
					state = HEADER_S2_WAIT;
					//Send S2 data
					WriteData(c2.GetData(), c2.GetSize());
					//Debug
					Log("-RTMPClientConnection::Sending c2.\n");
				}
				break;
			case HEADER_S2_WAIT:
				//Parse c2
				len = s2.Parse(buffer, bufferSize);
				//Move
				buffer += len;
				bufferSize -= len;
				//If it is parsed
				if (s2.IsParsed())
				{
					//Debug
					Log("-RTMPClientConnection::ParseData() Received s2. Sending connect.\n");
					//Update chunk size
					maxOutChunkSize = 4096;
					SendControlMessage(RTMPMessage::SetChunkSize, RTMPSetChunkSize::Create(maxOutChunkSize));
					
					//Params
					AMFObject* params = new AMFObject();
					//Add params
					params->AddProperty(L"app", appName);
					params->AddProperty(L"tcUrl", tcUrl);
					params->AddProperty(L"type", L"nonprivate");
					params->AddProperty(L"flasVer", L"FMLE3/0 (compatible; FMSc/1.0)");
					params->AddProperty(L"swfUrl", tcUrl);
					//Send connect message
					SendCommand(0, L"connect", params, new AMFNull(), [=](bool isError, AMFData* params, const std::vector<AMFData*>& extra){
						//Check if we need to authenticathe
						if (isError)
						{
							//Get error object
							RTMPNetStatusEvent* error = (RTMPNetStatusEvent*)params;
							//Check if we need auth
							if (RTMP::NetConnection::Connect::Rejected == error->GetCode())
							{
								//Check code
							}

							//Call listener
							listener->onError(this, RTMPClientConnection::ErrorCode::ConnectCommandFailed);
						} else {
							//Call listener
							listener->onConnected(this);
						}
					});
					//Move to next state
					state = CHUNK_HEADER_WAIT;
				}
				break;
			case CHUNK_HEADER_WAIT:
				//Parse header
				len = header.Parse(buffer, bufferSize);
				//Move
				buffer += len;
				bufferSize -= len;
				//If it is parsed
				if (header.IsParsed())
				{
					//Clean all buffers
					type0.Reset();
					type1.Reset();
					type2.Reset();
					extts.Reset();
					//Move to next state
					state = CHUNK_TYPE_WAIT;
					//Debug
					//Log("-RTMPClientConnection::Received header [fmt:%d,stream:%d]\n",header.GetFmt(),header.GetStreamId());
					//header.Dump();
				}
				break;
			case CHUNK_TYPE_WAIT:
				//Get sream id
				chunkStreamId = header.GetStreamId();
				//Find chunk stream
				it = chunkInputStreams.find(chunkStreamId);
				//Check if we have a new chunk stream or already got one
				if (it == chunkInputStreams.end())
				{
					//Log
					//Log("-RTMPClientConnection::Creating new chunk stream [id:%d]\n",chunkStreamId);
					//Create it
					chunkInputStream = new RTMPChunkInputStream();
					//Append it
					chunkInputStreams[chunkStreamId] = chunkInputStream;
				}
				else
					//Set the stream
					chunkInputStream = it->second;
				//Switch type
				switch (header.GetFmt())
				{
					case 0:
						//Check if the buffer type has been parsed
						len = type0.Parse(buffer, bufferSize);
						//Check if it is parsed
						if (type0.IsParsed())
						{
							//Debug
							//Debug("Got type 0 header [timestamp:%lu,messagelength:%d,type:%d,streamId:%d]\n",type0.GetTimestamp(),type0.GetMessageLength(),type0.GetMessageTypeId(),type0.GetMessageStreamId());
							//type0.Dump();
							//Set data for stream
							chunkInputStream->SetMessageLength(type0.GetMessageLength());
							chunkInputStream->SetMessageTypeId(type0.GetMessageTypeId());
							chunkInputStream->SetMessageStreamId(type0.GetMessageStreamId());
							//Check if we have extended timestamp
							if (type0.GetTimestamp() != 0xFFFFFF)
							{
								//Set timesptamp
								chunkInputStream->SetTimestamp(type0.GetTimestamp());
								//No timestamp delta
								chunkInputStream->SetTimestampDelta(0);
								//Move to next state
								state = CHUNK_DATA_WAIT;
							}
							else
								//We have to read 4 more bytes
								state = CHUNK_EXT_TIMESTAMP_WAIT;
							//Start data reception
							chunkInputStream->StartChunkData();
							//Reset sent bytes in buffer
							chunkLen = 0;
						}
						break;
					case 1:
						//Check if the buffer type has been parsed
						len = type1.Parse(buffer, bufferSize);
						//Check if it is parsed
						if (type1.IsParsed())
						{
							//Debug
							//Debug("Got type 1 header [timestampDelta:%u,messagelength:%d,type:%d]\n",type1.GetTimestampDelta(),type1.GetMessageLength(),type1.GetMessageTypeId());
							//type1.Dump();
							//Set data for stream
							chunkInputStream->SetMessageLength(type1.GetMessageLength());
							chunkInputStream->SetMessageTypeId(type1.GetMessageTypeId());
							//Check if we have extended timestam
							if (type1.GetTimestampDelta() != 0xFFFFFF)
							{
								//Set timestamp delta
								chunkInputStream->SetTimestampDelta(type1.GetTimestampDelta());
								//Set timestamp
								chunkInputStream->IncreaseTimestampWithDelta();
								//Move to next state
								state = CHUNK_DATA_WAIT;
							}
							else
								//We have to read 4 more bytes
								state = CHUNK_EXT_TIMESTAMP_WAIT;
							//Start data reception
							chunkInputStream->StartChunkData();
							//Reset sent bytes in buffer
							chunkLen = 0;
						}
						break;
					case 2:
						//Check if the buffer type has been parsed
						len = type2.Parse(buffer, bufferSize);
						//Check if it is parsed
						if (type2.IsParsed())
						{
							//Debug
							//Debug("Got type 2 header [timestampDelta:%lu]\n",type2.GetTimestampDelta());
							//type2.Dump();
							//Check if we have extended timestam
							if (type2.GetTimestampDelta() != 0xFFFFFF)
							{
								//Set timestamp delta
								chunkInputStream->SetTimestampDelta(type2.GetTimestampDelta());
								//Increase timestamp
								chunkInputStream->IncreaseTimestampWithDelta();
								//Move to next state
								state = CHUNK_DATA_WAIT;
							}
							else
								//We have to read 4 more bytes
								state = CHUNK_EXT_TIMESTAMP_WAIT;
							//Start data reception
							chunkInputStream->StartChunkData();
							//Reset sent bytes in buffer
							chunkLen = 0;
						}
						break;
					case 3:
						//Debug("Got type 3 header\n");
						//No header chunck
						len = 0;
						//If it is the first chunk
						if (chunkInputStream->IsFirstChunk())
							//Increase timestamp with previous delta
							chunkInputStream->IncreaseTimestampWithDelta();
						//Start data reception
						chunkInputStream->StartChunkData();
						//Move to next state
						state = CHUNK_DATA_WAIT;
						//Reset sent bytes in buffer
						chunkLen = 0;
						break;
				}
				//Move pointer
				buffer += len;
				bufferSize -= len;
				break;
			case CHUNK_EXT_TIMESTAMP_WAIT:
				//Parse extended timestamp
				len = extts.Parse(buffer, bufferSize);
				//Move
				buffer += len;
				bufferSize -= len;
				//If it is parsed
				if (extts.IsParsed())
				{
					//Check header type
					if (header.GetFmt() == 1)
					{
						//Set the timestamp
						chunkInputStream->SetTimestamp(extts.GetTimestamp());
						//No timestamp delta
						chunkInputStream->SetTimestampDelta(0);
					}
					else {
						//Set timestamp delta
						chunkInputStream->SetTimestampDelta(extts.GetTimestamp());
						//Increase timestamp
						chunkInputStream->IncreaseTimestampWithDelta();
					}
					//Move to next state
					state = CHUNK_DATA_WAIT;
				}
				break;
			case CHUNK_DATA_WAIT:
				//Check max buffer size
				if (maxChunkSize && chunkLen + bufferSize > maxChunkSize)
					//Parse only max chunk size
					len = maxChunkSize - chunkLen;
				else
					//parse all data
					len = bufferSize;
				//Check size
				if (!len)
				{
					//Debug
					Error("-RTMPClientConnection::ParseData()  Chunk data of size zero\n");
					//Skip
					break;
				}
				//Parse data
				len = chunkInputStream->Parse(buffer, len);
				//Check if it has parsed a msg
				if (chunkInputStream->IsParsed())
				{
					//Log("-RTMPClientConnection::Got message [timestamp:%lu]\n",chunkInputStream->GetTimestamp());
					//Get message
					RTMPMessage* msg = chunkInputStream->GetMessage();
					//Get message stream
					DWORD messageStreamId = msg->GetStreamId();
					//Check message type
					if (msg->IsControlProtocolMessage())
					{
						//Get message type
						BYTE type = msg->GetType();
						//Get control protocl message
						RTMPObject* ctrl = msg->GetControlProtocolMessage();
						//Procces msg
						ProcessControlMessage(messageStreamId, type, ctrl);
					}
					else if (msg->IsCommandMessage()) {
						//Get Command message
						RTMPCommandMessage* cmd = msg->GetCommandMessage();
						//Proccess msg
						ProcessCommandMessage(messageStreamId, cmd);
					}
					else if (msg->IsMedia()) {
						//Get media frame
						RTMPMediaFrame* frame = msg->GetMediaFrame();
						//Check if we have it
						if (frame)
							//Process message
							ProcessMediaData(messageStreamId, frame);
					}
					else if (msg->IsMetaData() || msg->IsSharedObject()) {
						//Get object
						RTMPMetaData* meta = msg->GetMetaData();
						//Process meta data
						ProcessMetaData(messageStreamId, meta);
					}
					else {
						//UUh??
						Error("-RTMPClientConnection::ParseData() Unknown rtmp message, should never happen\n");
					}
					//Delete msg
					delete(msg);
					//Move to next chunck
					state = CHUNK_HEADER_WAIT;
					//Clean header
					header.Reset();
				}
				//Increase buffer length
				chunkLen += len;
				//Move pointer
				buffer += len;
				bufferSize -= len;
				//Check max chunk size
				if (maxChunkSize && chunkLen >= maxChunkSize)
				{
					//Wait for next buffer header
					state = CHUNK_HEADER_WAIT;
					//Clean header
					header.Reset();
				}

				break;
			case NONE:
				//We should not be here
				break;
		}
	}
}

/***********************
 * WriteData
 *	Write data to socket
 ***********************/
int RTMPClientConnection::WriteData(BYTE* data, const DWORD size)
{
	//Write it
	return write(fd, data, size);
}

void RTMPClientConnection::ProcessControlMessage(DWORD streamId, BYTE type, RTMPObject* msg)
{
	Log("-RTMPClientConnection::ProcessControlMessage() [streamId:%d,type:%s]\n", streamId, RTMPMessage::TypeToString((RTMPMessage::Type)type));

	RTMPUserControlMessage* event;

	//Check type
	switch ((RTMPMessage::Type)type)
	{
		case RTMPMessage::SetChunkSize:
			//Get new chunk size
			maxChunkSize = ((RTMPSetChunkSize*)msg)->GetChunkSize();
			Log("-RTMPClientConnection::ProcessControlMessage() Set new chunk size [%d]\n", maxChunkSize);
			break;
		case RTMPMessage::AbortMessage:
			Log("-RTMPClientConnection::ProcessControlMessage() AbortMessage [chunkId:%d]\n", ((RTMPAbortMessage*)msg)->GetChunkStreamId());
			break;
		case RTMPMessage::Acknowledgement:
			Log("-RTMPClientConnection::ProcessControlMessage() Acknowledgement [seq:%d]\n", ((RTMPAcknowledgement*)msg)->GetSeNumber());
			break;
		case RTMPMessage::UserControlMessage:
			//Get event
			event = (RTMPUserControlMessage*)msg;
			//Depending on the event received
			switch (event->GetEventType())
			{
			case RTMPUserControlMessage::StreamBegin:
				Log("-RTMPClientConnection::ProcessControlMessage() StreamBegin [stream:%d]\n", event->GetEventData());
				break;
			case RTMPUserControlMessage::StreamEOF:
				Log("-RTMPClientConnection::ProcessControlMessage() StreamEOF [stream:%d]\n", event->GetEventData());
				break;
			case RTMPUserControlMessage::StreamDry:
				Log("-RTMPClientConnection::ProcessControlMessage() StreamDry [stream:%d]\n", event->GetEventData());
				break;
			case RTMPUserControlMessage::SetBufferLength:
				Log("-RTMPClientConnection::ProcessControlMessage() SetBufferLength [stream:%d,size:%d]\n", event->GetEventData(), event->GetEventData2());
				break;
			case RTMPUserControlMessage::StreamIsRecorded:
				Log("-RTMPClientConnection::ProcessControlMessage() StreamIsRecorded [stream:%d]\n", event->GetEventData());
				break;
			case RTMPUserControlMessage::PingRequest:
				Log("-RTMPClientConnection::ProcessControlMessage() PingRequest [milis:%d]\n", event->GetEventData());
				//Send ping response
				SendControlMessage(RTMPMessage::UserControlMessage, RTMPUserControlMessage::CreatePingResponse(0));
				break;
			case RTMPUserControlMessage::PingResponse:
				Log("-RTMPClientConnection::ProcessControlMessage() PingResponse [milis:%d]\n", event->GetEventData());
				break;

			}
			break;
		case RTMPMessage::WindowAcknowledgementSize:
			//Store new acknowledgement size
			windowSize = ((RTMPWindowAcknowledgementSize*)msg)->GetWindowSize();
			Log("-RTMPClientConnection::ProcessControlMessage() WindowAcknowledgementSize [%d]\n", windowSize);
			break;
		case RTMPMessage::SetPeerBandwidth:
			Log("-RTMPClientConnection::ProcessControlMessage() SetPeerBandwidth\n");
			break;
		default:
			Log("-RTMPClientConnection::ProcessControlMessage() Unknown [type:%d]\n", type);
			break;
	}
}

/********************************
 * ProcessCommandMessage
 *
 ************************************/
void RTMPClientConnection::ProcessCommandMessage(DWORD streamId, RTMPCommandMessage* cmd)
{
	//Get message values
	std::wstring name = cmd->GetName();
	QWORD transId = cmd->GetTransId();
	AMFData* params = cmd->GetParams();
	auto& extra = cmd->GetExtra();

	
	//If it is not a result
	if (name.compare(L"_error") == 0 || name.compare(L"_result") == 0)
	{

		//Log
		Log("-RTMPClientConnection::ProcessCommandMessage() Got response [streamId:%d,name:\"%ls\",transId:%ld]\n", streamId, name.c_str(), transId);
		cmd->Dump();

		//Check if it is an errror
		bool isError = name.compare(L"_error") == 0;

		//Find transaction
		const auto it = transactions.find(transId);

		//If not found
		if (it == transactions.end())
		{
			//Error
			Error("Transaction not found [%llu]\n", transId);
			//Exit
			return;
		} 
		//Call listener
		it->second(isError, params, extra);

		//Erase transaction
		transactions.erase(it);
	} else {
		//Log
		Log("-RTMPClientConnection::ProcessCommandMessage() Got command [streamId:%d,name:\"%ls\",transId:%ld]\n", streamId, name.c_str(), transId);
		cmd->Dump();

		//Call listener
		listener->onCommand(this, streamId, name.c_str(), params, extra);
	} 

}

DWORD RTMPClientConnection::SendCommand(DWORD streamId, const wchar_t* name, AMFData* params, AMFData* extra, std::function<void(bool, AMFData*, const std::vector<AMFData*>&)> callback)
{
	//Send command
	QWORD transId = SendCommand(streamId, name, params, extra);
	//Add transaction
	transactions[transId] = callback;
	//Return id
	return transId;
}



void RTMPClientConnection::ProcessMediaData(DWORD streamId, RTMPMediaFrame* frame)
{
}

void RTMPClientConnection::ProcessMetaData(DWORD streamId, RTMPMetaData* meta)
{
}

DWORD RTMPClientConnection::SendCommand(DWORD streamId, const wchar_t* name, AMFData* params, AMFData* extra)
{
	Log("-RTMPClientConnection::SendCommand() [streamId:%d,name:%ls]\n", streamId, name);
	//Get transId
	QWORD transId = ++maxTransId;
	//Create cmd response
	RTMPCommandMessage* cmd = new RTMPCommandMessage(name, transId, params, extra);
	//Dump
	cmd->Dump();
	//Get timestamp
	QWORD ts = getDifTime(&startTime) / 1000;
	//Append message to command stream
	chunkOutputStreams[3]->SendMessage(new RTMPMessage(streamId, ts, cmd));
	//We have new data to send
	SignalWriteNeeded();
	//Return id
	return transId;
}

void RTMPClientConnection::SendCommandResponse(DWORD streamId, const wchar_t* name, QWORD transId, AMFData* params, AMFData* extra)
{
	Log("-RTMPClientConnection::SendCommandResponse() [streamId:%d,name:%ls,transId:%ld]\n", streamId, name, transId);
	//Create cmd response
	RTMPCommandMessage* cmd = new RTMPCommandMessage(name, transId, params, extra);
	//Dump
	cmd->Dump();
	//Get timestamp
	QWORD ts = getDifTime(&startTime) / 1000;
	//Append message to command stream
	chunkOutputStreams[3]->SendMessage(new RTMPMessage(streamId, ts, cmd));
	//We have new data to send
	SignalWriteNeeded();
}

void RTMPClientConnection::SendCommandResult(DWORD streamId, QWORD transId, AMFData* params, AMFData* extra)
{
	SendCommandResponse(streamId, L"_result", transId, params, extra);
}

void RTMPClientConnection::SendCommandError(DWORD streamId, QWORD transId, AMFData* params, AMFData* extra)
{
	SendCommandResponse(streamId, L"_error", transId, params, extra);
}

void RTMPClientConnection::SendControlMessage(RTMPMessage::Type type, RTMPObject* msg)
{

	Log("-RTMPClientConnection::SendControlMessage() [%s]\n", RTMPMessage::TypeToString(type));

	//Get timestamp
	QWORD ts = getDifTime(&startTime) / 1000;
	//Append message to control stream
	chunkOutputStreams[2]->SendMessage(new RTMPMessage(0, ts, type, msg));
	//We have new data to send
	SignalWriteNeeded();
}

/****************************************
 * RTMPStreamListener events
 *
 ****************************************/
void RTMPClientConnection::onAttached(RTMPMediaStream* stream)
{
	Debug("-RTMPClientConnection::onAttached()\n");
}

void RTMPClientConnection::onDetached(RTMPMediaStream* stream)
{
	Debug("-RTMPClientConnection::onDetached()\n");
}

void RTMPClientConnection::onStreamBegin(DWORD streamId)
{
	Debug("-RTMPClientConnection::onStreamBegin()\n");

	//Send control message
	SendControlMessage(RTMPMessage::UserControlMessage, RTMPUserControlMessage::CreateStreamBegin(streamId));
}

void RTMPClientConnection::onStreamEnd(DWORD streamId)
{
	Debug("-RTMPClientConnection::onStreamEnd()\n");

	//Send control message
	SendControlMessage(RTMPMessage::UserControlMessage, RTMPUserControlMessage::CreateStreamEOF(streamId));
}

void RTMPClientConnection::onCommand(DWORD streamId, const wchar_t* name, AMFData* obj)
{
	Debug("-RTMPClientConnection::onCommand() [name:%ls]\n", name);
	if (obj) obj->Dump();

	//Send new command
	SendCommand(streamId, name, new AMFNull(), obj);
}

void RTMPClientConnection::onMediaFrame(DWORD streamId, RTMPMediaFrame* frame)
{
	//Get the timestamp from the frame
	QWORD ts = frame->GetTimestamp();

	//Check timestamp
	if (ts == (QWORD)-1)
		//Calculate timestamp based on current time
		ts = getDifTime(&startTime) / 1000;

	//Dependign on the streams
	switch (frame->GetType())
	{
		case RTMPMediaFrame::Audio:
			//Append to the audio trunk
			chunkOutputStreams[4]->SendMessage(new RTMPMessage(streamId, ts, frame->Clone()));
			break;
		case RTMPMediaFrame::Video:
			chunkOutputStreams[5]->SendMessage(new RTMPMessage(streamId, ts, frame->Clone()));
			break;
	}
	//Signal frames
	SignalWriteNeeded();
}

void RTMPClientConnection::onMetaData(DWORD streamId, RTMPMetaData* meta)
{
	Debug("-RTMPClientConnection::onMetaData()\n");

	//Get the timestamp of the metadata
	QWORD ts = meta->GetTimestamp();

	//Check timestamp
	if (ts == (QWORD)-1)
		//Calculate timestamp based on current time
		ts = getDifTime(&startTime) / 1000;

	//Append to the comand trunk
	chunkOutputStreams[3]->SendMessage(new RTMPMessage(streamId, ts, meta->Clone()));
	//Signal frames
	SignalWriteNeeded();
}

void RTMPClientConnection::onStreamReset(DWORD id)
{
	Debug("-RTMPClientConnection::onStreamReset()\n");


	for (RTMPChunkOutputStreams::iterator it = chunkOutputStreams.begin(); it != chunkOutputStreams.end(); ++it)
	{
		//Get stream
		RTMPChunkOutputStream* chunkOutputStream = it->second;
		//Get chunk id
		DWORD chunkId = it->first;

		//Check it it has data pending
		if (chunkOutputStream->ResetStream(id))
			//Send Abort message
			SendControlMessage(RTMPMessage::AbortMessage, RTMPAbortMessage::Create(chunkId));
	}
}
