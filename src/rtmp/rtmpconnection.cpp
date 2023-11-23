#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <thread>
#include "log.h"
#include "assertions.h"
#include "tools.h"
#include "rtmp/rtmphandshake.h"
#include "rtmp/rtmpconnection.h"

constexpr int PoolTimeout = 30E3; //30s

/********************************
 * RTMP connection demultiplex buffers streams from incoming raw data
 * extracting the individual buffers and passes the message fragments
 * to the message layer.
 *******************************************************************/

RTMPConnection::RTMPConnection(Listener *listener)
{
	//Set initial state
	state = HEADER_C0_WAIT;
	//Set chunk size
	maxChunkSize = 128;
	maxOutChunkSize = 128;
	//Byte counters
	inBytes = 0;
	outBytes = 0;
	windowSize = 0;
	curWindowSize = 0;
	recvSize = 0;
	//Not encripted by default
	digest = false;
	//Store listener
	this->listener = listener;
	//No media
	app = NULL;
	//Set first media id
	maxStreamId = 1;
	maxTransId = 1;
	//Not inited
	inited = false;
	running = false;
	socket = FD_INVALID;
	//Set initial time
	gettimeofday(&startTime,0);
	//Init mutex
	pthread_mutex_init(&mutex,0);
	//Create output chunk streams for control
	chunkOutputStreams[2] = new RTMPChunkOutputStream(2);
	//Create output chunk streams for command
	chunkOutputStreams[3] = new RTMPChunkOutputStream(3);
	//Create output chunk streams for audio
	chunkOutputStreams[4] = new RTMPChunkOutputStream(4);
	//Create output chunk streams for video
	chunkOutputStreams[5] = new RTMPChunkOutputStream(5);
}

RTMPConnection::~RTMPConnection()
{
	Log("-RTMPConnection::~RTMPConnection() [%p]\n",this);
	//End just in case
	End();
	//For each chunk strean
	for (RTMPChunkInputStreams::iterator it=chunkInputStreams.begin(); it!=chunkInputStreams.end(); ++it)
		//Delete it
		delete(it->second);
	//For each chunk strean
	for (RTMPChunkOutputStreams::iterator it=chunkOutputStreams.begin(); it!=chunkOutputStreams.end(); ++it)
		//Delete it
		delete(it->second);
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

int RTMPConnection::Init(int fd)
{
	Log(">RTMP Connection init [%d]\n",fd);

	//Store socket
	socket = fd;

	//I am inited
	inited = true;

	//Start
	Start();

	Log("<RTMP Connection init\n");

	return 1;
}

void RTMPConnection::Start()
{
	//We are running
	running = true;
	
	//Start thread and run, hold reference to us to prevent being destroyed before Run ends.
	thread = std::thread([=, self=shared_from_this()](){
		//Block signals to avoid exiting on SIGUSR1
		blocksignals();
		//Run
		Run();
	});
}

void RTMPConnection::Stop()
{
	//If got socket
	if (running)
	{
		//Not running;
		running = false;
		//Close socket
		shutdown(socket,SHUT_RDWR);
		//Will cause poll to return
		MCU_CLOSE(socket);
	}
}

int RTMPConnection::End()
{
	//Check we have been inited
	if (!inited)
		//Exit
		return 0;

	Log(">RTMPConnection::End()\n");

	//Not inited any more
	inited = false;

	//Stop just in case
	Stop();

	//If thread is already running
	if (thread.joinable())
	{
		//If we are on different thread
		if (std::this_thread::get_id()!=thread.get_id())
			//Join it
			thread.join();
		else
			//Detach as we are ending ourselves
			thread.detach();
	}

	//Ended
	Log("<RTMPConnection::End()\n");

	return 1;
}


/***************************
 * Run
 * 	Server running thread
 ***************************/
int RTMPConnection::Run()
{
	BYTE data[1400];
	unsigned int size = 1400;

	Log(">RTMPConnection::Run() [connection:%p]\n",this);

	//Set values for polling
	ufds[0].fd = socket;
	ufds[0].events = POLLIN | POLLERR | POLLHUP;

	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(socket,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	(void)fcntl(socket,F_SETFL,fsflags);

	//Set no delay option
	int flag = 1;
        (void)setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
	//Catch all IO errors
	signal(SIGIO, EmptyCatch);
	signal(SIGPIPE, EmptyCatch);

	//Run until ended
	while(running)
	{
		//Wait for events
		int ret = poll(ufds,1,PoolTimeout);
		
		//If there was an error
		if (ret<0)
			//Check again
			continue;
		
		//If timed out
		if (ret==0)
		{
			//Log and exit run loop
			Log("-RTMPConnection::Run() Timedout [connection:%p]\n",this);
			break;
		}
			
		if (ufds[0].revents & POLLOUT)
		{
			//Write data buffer
			DWORD len = SerializeChunkData(data,size);
			//Check length
			if (len)
			{
				//Send it
				WriteData(data,len);
				//Increase sent bytes
				outBytes += len;
			}
		}

		if (ufds[0].revents & POLLIN)
		{
			//Read data from connection
			int len = read(socket,data,size);
			if (len<=0)
			{
				//Error
				Log("Readed [%d,%d]\n",len,errno);
				//Exit
				break;
			}
			//Increase in bytes
			inBytes += len;

			try {
				//Parse data
				ParseData(data,len);
			} catch (std::exception &e) {
				//Show error
				Error("Exception parsing data: %s\n",e.what());
				//Dump it
				Dump(data,len);
				//Break on any error
				break;
			}
		}

		if ((ufds[0].revents & POLLHUP) || (ufds[0].revents & POLLERR))
		{
			//Error
			Log("Pool error event [%d]\n",ufds[0].revents);
			//Exit
			break;
		}
	}
	
	Log("-RTMPConnection::Run() Disconnecting [connection:%p]\n",this);

	//If got application
	if (app)
	{
		//lock now
		pthread_mutex_lock(&mutex);

		//Disconnect all streams
		for (auto it=streams.begin(); it!=streams.end(); ++it)
			//Delete stream
			app->DeleteStream(it->second);
		
		//Clear stream
		streams.clear();

		//Unlock
		pthread_mutex_unlock(&mutex);

		//Disconnect application
		app->RemoveListener(this);
		//Disconnected
		app->Disconnected();
	}
	
	//Check listener
	if (listener)
		//launch event
		listener->onDisconnect(shared_from_this());
	
	Log("<RTMPConnection::Run() Disconnected [connection:%p]\n",this);

	//Done
	return 1;
}

void RTMPConnection::SignalWriteNeeded()
{
	//lock now
	pthread_mutex_lock(&mutex);

	//Check if there was not anyhting left in the queeue
	if (!(ufds[0].events & POLLOUT))
	{
		//Init bandwidth calculation
		bandIni = getDifTime(&startTime);
		//Nothing sent
		bandSize = 0;
	}

	//Set to wait also for read events
	ufds[0].events = POLLIN | POLLOUT | POLLERR | POLLHUP;

	//Unlock
	pthread_mutex_unlock(&mutex);

	//Signal the pthread this will cause the poll call to exit
	pthread_kill(thread.native_handle(),SIGIO);
}

DWORD RTMPConnection::SerializeChunkData(BYTE *data,DWORD size)
{
	DWORD len = 0;

	//Lock mutex
	pthread_mutex_lock(&mutex);

	//Remove the write signal
	//ufds[0].events = POLLIN | POLLERR | POLLHUP;

	//Iterate the chunks in ascendig order (more important firsts)
	for (RTMPChunkOutputStreams::iterator it=chunkOutputStreams.begin(); it!=chunkOutputStreams.end();++it)
	{
		//Get stream
		RTMPChunkOutputStream* chunkOutputStream = it->second;

		//Check it it has data pending
		while (chunkOutputStream->HasData())
		{
			//Check if we do not have enought space left for more
			if(size-len<maxOutChunkSize+12)
			{
				//We have more data to write
				//ufds[0].events = POLLIN | POLLOUT | POLLERR | POLLHUP;
				//End this writing
				goto end;
			}

			//Write next chunk from this stream
			len += chunkOutputStream->GetNextChunk(data+len,size-len,maxOutChunkSize);

		}
	}


end:
	//Add size
	bandSize += len;
	//Calc elapsed time
	QWORD elapsed = getDifTime(&startTime)-bandIni;
	//Check if
	if (!len)
	{
		//Do not wait for write anymore
		ufds[0].events = POLLIN | POLLERR | POLLHUP;

		//Check
		if (elapsed)
		{
			//Calculate bandwith in kbps
			bandCalc = bandSize*8000/elapsed;
			//LOg
			//Debug("-Band calc ended [%d,%dkbps in %dns]\n",bandSize,bandCalc,elapsed);
		}
	} else {
		//Check
		if (elapsed>1000000)
		{
			//Calculate bandwith in kbps
			bandCalc = bandSize*8000/elapsed;
			//LOg
			//Debug("-Band calc resetd [%d,%dkbps in %dns]\n",bandSize,bandCalc,elapsed);
			bandIni = getDifTime(&startTime);
			bandSize = 0;
		}
	}

	//Un Lock mutex
	pthread_mutex_unlock(&mutex);

	//Return chunks data length
	return len;
}

/***********************
 * ParseData
 * 	Process incomming data
 **********************/
void RTMPConnection::ParseData(BYTE *data,const DWORD size)
{
	RTMPChunkInputStreams::iterator it;
	int len = 0;
	int digesOffsetMethod = 0;

	//Get pointer and data size
	BYTE *buffer = data;
	DWORD bufferSize = size;

	//Increase current window
	curWindowSize += size;
	//And total size
	recvSize += size;

	//Check current window
	if (windowSize && curWindowSize>windowSize)
	{
		//Send
		SendControlMessage(RTMPMessage::Acknowledgement,RTMPAcknowledgement::Create(recvSize));
		//Reset window
		curWindowSize = 0;
	}

	//While there is data
	while(bufferSize>0)
	{
		//Check connection state
		switch(state)
		{
			case HEADER_C0_WAIT:
				//Parse c0
				len = c0.Parse(buffer,bufferSize);
				//Move
				buffer+=len;
				bufferSize-=len;
				//If it is parsed
				if (c0.IsParsed())
				{
					//Move to next state
					state = HEADER_C1_WAIT;
					//Debug
					Log("Received c0 version: %d\n",c0.GetRTMPVersion());
				}
				break;
			case HEADER_C1_WAIT:
				//Parse c1
				len = c1.Parse(buffer,bufferSize);
				//Move
				buffer+=len;
				bufferSize-=len;
				//If it is parsed
				if (c1.IsParsed())
				{
					Log("-Received C1 client version [%d,%d,%d,%d]\n",c1.GetVersion()[0],c1.GetVersion()[1],c1.GetVersion()[2],c1.GetVersion()[3]);
					//Set s0 data
					s01.SetRTMPVersion(3);
					//Set current timestamp
					s01.SetTime(getDifTime(&startTime)/1000);
					//Check which version are we using
					if (c1.GetVersion()[3])
					{
						//Verify client
						digesOffsetMethod = VerifyC1Data(c1.GetData(),c1.GetSize());
						//Set version
						s01.SetVersion(3,5,1,1);
						//Check if found diggest ofset
						digest = (digesOffsetMethod>0);
					} else {
						//Seet version to zero
						s01.SetVersion(0,0,0,0);
						//Do not calculate digest
						digest = false;
					}
					//Set random data from memory
					BYTE* random = s01.GetRandom();
					//Fill it
					for (size_t i=0;i<s01.GetRandomSize();i++)
						//With random
						random[i] = rand();
					//If we have to calculate digest
					if (digest)
						//calculate digest for s1 only, skipping s0
						GenerateS1Data(digesOffsetMethod,s01.GetData()+1,s01.GetSize()-1);
					//Send S01 data
					(void)WriteData(s01.GetData(),s01.GetSize());
					//Move to next state
					state = HEADER_C2_WAIT;
					//Debug
					Log("Sending s0 and s1 with digest %s offset method %d\n",digest?"on":"off",digesOffsetMethod);
					//Set s2 data
					s2.SetTime(c1.GetTime());
					//Set current timestamp
					s2.SetTime2(getDifTime(&startTime)/1000);
					//Echo c1 data
					s2.SetRandom(c1.GetRandom(),c1.GetRandomSize());
					//If we have to calculate digest
					if (digest)
						//calculate digest for s1
						GenerateS2Data(digesOffsetMethod,s2.GetData(),s2.GetSize());
					//Send S2 data
					(void)WriteData(s2.GetData(),s2.GetSize());
					//Debug
					Log("Sending c2.\n");
				}
				break;
			case HEADER_C2_WAIT:
				//Parse c2
				len = c2.Parse(buffer,bufferSize);
				//Move
				buffer+=len;
				bufferSize-=len;
				//If it is parsed
				if (c2.IsParsed())
				{
					//Move to next state
					state = CHUNK_HEADER_WAIT;
					//Debug
					Log("Received c2. CONNECTED.\n");
				}
				break;
			case CHUNK_HEADER_WAIT:
				//Parse c2
				len = header.Parse(buffer,bufferSize);
				//Move
				buffer+=len;
				bufferSize-=len;
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
					//Log("Received header [fmt:%d,stream:%d]\n",header.GetFmt(),header.GetStreamId());
					//header.Dump();
				}
				break;
			case CHUNK_TYPE_WAIT:
				//Get sream id
				chunkStreamId = header.GetStreamId();
				//Find chunk stream
				it = chunkInputStreams.find(chunkStreamId);
				//Check if we have a new chunk stream or already got one
				if (it==chunkInputStreams.end())
				{
					//Log
					//Log("Creating new chunk stream [id:%d]\n",chunkStreamId);
					//Create it
					chunkInputStream = new RTMPChunkInputStream();
					//Append it
					chunkInputStreams[chunkStreamId] = chunkInputStream;
				} else
					//Set the stream
					chunkInputStream = it->second;
				//Switch type
				switch(header.GetFmt())
				{
					case 0:
						//Check if the buffer type has been parsed
						len = type0.Parse(buffer,bufferSize);
						//Check if it is parsed
						if (type0.IsParsed())
						{
							//Debug
							//Debug("Got type 0 header [timestamp:%lu,messagelength:%d,type:%d,streamId:%d]\n",type0.GetTimestamp(),type0.GetMessageLength(),type0.GetMessageTypeId(),type0.GetMessageStreamId());
							//type0.Dump();
							//Set data for stream
							chunkInputStream->SetMessageLength	(type0.GetMessageLength());
							chunkInputStream->SetMessageTypeId	(type0.GetMessageTypeId());
							chunkInputStream->SetMessageStreamId	(type0.GetMessageStreamId());
							//Check if we have extended timestamp
							chunkInputStream->SetIsExtendedTimestamp(type0.GetTimestamp() == 0xFFFFFF);
							if (!chunkInputStream->IsExtendedTimestamp())
							{
								//Set timesptamp
								chunkInputStream->SetTimestamp(type0.GetTimestamp());
								//No timestamp delta
								chunkInputStream->SetTimestampDelta(0);
								//Move to next state
								state = CHUNK_DATA_WAIT;
							} else
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
						len = type1.Parse(buffer,bufferSize);
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
							chunkInputStream->SetIsExtendedTimestamp(type1.GetTimestampDelta() == 0xFFFFFF);
							if (!chunkInputStream->IsExtendedTimestamp())
							{
								//Set timestamp delta
								chunkInputStream->SetTimestampDelta(type1.GetTimestampDelta());
								//Set timestamp
								chunkInputStream->IncreaseTimestampWithDelta();
								//Move to next state
								state = CHUNK_DATA_WAIT;
							} else
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
						len = type2.Parse(buffer,bufferSize);
						//Check if it is parsed
						if (type2.IsParsed())
						{
							//Debug
							//Debug("Got type 2 header [timestampDelta:%lu]\n",type2.GetTimestampDelta());
							//type2.Dump();
							//Check if we have extended timestam							
							chunkInputStream->SetIsExtendedTimestamp(type2.GetTimestampDelta() == 0xFFFFFF);
							if (!chunkInputStream->IsExtendedTimestamp())
							{
								//Set timestamp delta
								chunkInputStream->SetTimestampDelta(type2.GetTimestampDelta());
								//Increase timestamp
								chunkInputStream->IncreaseTimestampWithDelta();
								//Move to next state
								state = CHUNK_DATA_WAIT;
							} else
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
						state = chunkInputStream->IsExtendedTimestamp() ? CHUNK_EXT_TIMESTAMP_WAIT : CHUNK_DATA_WAIT;
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
				len = extts.Parse(buffer,bufferSize);
				//Move
				buffer+=len;
				bufferSize-=len;
				//If it is parsed
				if (extts.IsParsed())
				{
					//Check header type
					if (header.GetFmt()==1)
					{
						//Set the timestamp
						chunkInputStream->SetTimestamp(extts.GetTimestamp());
						//No timestamp delta
						chunkInputStream->SetTimestampDelta(0);
					} else {
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
				if (maxChunkSize && chunkLen+bufferSize>maxChunkSize)
					//Parse only max chunk size
					len = maxChunkSize-chunkLen;
				else
					//parse all data
					len = bufferSize;
				//Check size
				if (!len)
				{
					//Debug
					Error("Chunk data of size zero\n");
					//Skip
					break;
				}
				//Parse data
				len = chunkInputStream->Parse(buffer,len);
				//Check if it has parsed a msg
				if (chunkInputStream->IsParsed())
				{
					//Log("Got message [timestamp:%lu]\n",chunkInputStream->GetTimestamp());
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
						ProcessControlMessage(messageStreamId,type,ctrl);
					} else if (msg->IsCommandMessage()) {
						//Get Command message
						RTMPCommandMessage* cmd = msg->GetCommandMessage();
						//Proccess msg
						ProcessCommandMessage(messageStreamId,cmd);
					} else if (msg->IsMedia()) {
						//Get media frame
						RTMPMediaFrame* frame = msg->GetMediaFrame();
						//Check if we have it
						if (frame)
						{
							//Process message
							ProcessMediaData(messageStreamId,frame);
						}
						else
						{
							Error("Failed to get media frame\n");
						}
					} else if (msg->IsMetaData() || msg->IsSharedObject()) {
						//Get object
						RTMPMetaData *meta = msg->GetMetaData();
						//Process meta data
						ProcessMetaData(messageStreamId,meta);
					} else {
						//UUh??
						Error("Unknown rtmp message, should never happen\n");
					}
					//Delete msg
					delete(msg);
					//Move to next chunck
					state = CHUNK_HEADER_WAIT;
					//Clean header
					header.Reset();
				} else if (!len) {
					throw std::runtime_error("Could not parse message");
				}
				//Increase buffer length
				chunkLen += len;
				//Move pointer
				buffer += len;
				bufferSize -= len;
				//Check max chunk size
				if (maxChunkSize && chunkLen>=maxChunkSize)
				{
					//Wait for next buffer header
					state = CHUNK_HEADER_WAIT;
					//Clean header
					header.Reset();
				}

				break;
		}
	}
}

/***********************
 * WriteData
 *	Write data to socket
 ***********************/
int RTMPConnection::WriteData(BYTE *data,const DWORD size)
{
#ifdef DEBUG
	/*char name[256];
	sprintf(name,"/tmp/mcu_out_%p.raw",this);
	int fd = open(name,O_CREAT|O_WRONLY|O_APPEND, S_IRUSR | S_IWUSR, 0644);
	write(fd,data,size);
	MCU_CLOSE(fd);*/
#endif
	return send(socket, data, size, MSG_NOSIGNAL);
}

void RTMPConnection::ProcessControlMessage(DWORD streamId,BYTE type,RTMPObject* msg)
{
	Log("-ProcessControlMessage [streamId:%d,type:%s]\n",streamId,RTMPMessage::TypeToString((RTMPMessage::Type)type));

	 RTMPUserControlMessage *event;

	//Check type
	switch((RTMPMessage::Type)type)
	{
		case RTMPMessage::SetChunkSize:
			//Get new chunk size
			maxChunkSize = ((RTMPSetChunkSize *)msg)->GetChunkSize();
			Log("-Set new chunk size [%d]\n",maxChunkSize);
			break;
		case RTMPMessage::AbortMessage:
			Log("AbortMessage [chunkId:%d]\n",((RTMPAbortMessage*)msg)->GetChunkStreamId());
			break;
		case RTMPMessage::Acknowledgement:
			Log("Acknowledgement [seq:%d]\n",((RTMPAcknowledgement*)msg)->GetSeNumber());
			break;
		case RTMPMessage::UserControlMessage:
			//Get event
			event = (RTMPUserControlMessage*)msg;
			//Depending on the event received
			switch(event->GetEventType())
			{
				case RTMPUserControlMessage::StreamBegin:
					Log("StreamBegin [stream:%d]\n",event->GetEventData());
					break;
				case RTMPUserControlMessage::StreamEOF:
					Log("StreamEOF [stream:%d]\n",event->GetEventData());
					break;
				case RTMPUserControlMessage::StreamDry:
					Log("StreamDry [stream:%d]\n",event->GetEventData());
					break;
				case RTMPUserControlMessage::SetBufferLength:
					Log("SetBufferLength [stream:%d,size:%d]\n",event->GetEventData(),event->GetEventData2());
					break;
				case RTMPUserControlMessage::StreamIsRecorded:
					Log("StreamIsRecorded [stream:%d]\n",event->GetEventData());
					break;
				case RTMPUserControlMessage::PingRequest:
					Log("PingRequest [milis:%d]\n",event->GetEventData());
					//Send ping response
					SendControlMessage(RTMPMessage::UserControlMessage,RTMPUserControlMessage::CreatePingResponse(event->GetEventData()));
					break;
				case RTMPUserControlMessage::PingResponse:
				{
					//Get response
					DWORD ping = event->GetEventData();
					//Get roundtrip delay
					rtt = getDifTime(&startTime)/1000-ping;
					
					//Check if a stream has been created with that id
					pthread_mutex_lock(&mutex);
					for (auto const &it : streams)
						it.second->SetRTT(rtt);
					pthread_mutex_unlock(&mutex);
					
					Log("PingResponse [ping:%d,delay:%d]\n",ping,rtt);
					break;
				}

			}
			break;
		case RTMPMessage::WindowAcknowledgementSize:
			//Store new acknowledgement size
			windowSize = ((RTMPWindowAcknowledgementSize*)msg)->GetWindowSize();
			Log("WindowAcknowledgementSize [%d]\n",windowSize);
			break;
		case RTMPMessage::SetPeerBandwidth:
			Log("SetPeerBandwidth\n");
			break;
		default:
			Log("Unknown [type:%d]\n",type);
			break;
	}
}

/********************************
 * ProcessCommandMessage
 *
 ************************************/
void RTMPConnection::ProcessCommandMessage(DWORD streamId,RTMPCommandMessage* cmd)
{
	//Ensure we have name and trans if
	if (!cmd->HasName() || !cmd->HasTransId())
	{
		//Error
		Log("-RTMPConnection::ProcessCommandMessage() | Command does not have name or transid\n");
		//Skip
		return;
	}
	
	//Get message values
	std::wstring name 	= cmd->GetName();
	QWORD transId 		= cmd->GetTransId();
	AMFData* params 	= cmd->GetParams();

	//Log
	Log("-RTMPConnection::ProcessCommandMessage() [streamId:%d,name:\"%ls\",transId:%ld]\n",streamId,name.c_str(),transId);

	//Check message Stream
	if (streamId)
	{
		//Lock mutex
		pthread_mutex_lock(&mutex);
		
		//Check if a stream has been created with that id
		RTMPNetStreams::iterator it = streams.find(streamId);

		//If not found
		if (it==streams.end())
		{
			//Unnock mutex
			pthread_mutex_unlock(&mutex);
			//Send error
			return SendCommandError(streamId,transId,NULL,NULL);
		}

		//Get media stream
		auto stream = it->second;

		//Ensure valid
		if (!stream)
		{
			//Unnock mutex
			pthread_mutex_unlock(&mutex);
			//Send error
			return SendCommandError(streamId,transId,NULL,NULL);
		}

		//Let it process the message
		stream->ProcessCommandMessage(cmd);
		
		//Lock mutex
		pthread_mutex_unlock(&mutex);

	} else if (name.compare(L"connect")==0) {
		double objectEncoding = 0;
		//Check if we already have an active media stream application
		if (app)
			//Send error
			return SendCommandError(streamId,transId);

		//Check we have params
		if (!params || !params->CheckType(AMFData::Object))
			//Send error
			return SendCommandError(streamId,transId);

		//Get object
		AMFObject *obj = (AMFObject*)params;
		//Check if we have an app
		if (!obj->HasProperty(L"app"))
			//Send error
			return SendCommandError(streamId,transId);

		//Get client address
		struct sockaddr_in peername;
		socklen_t peernameLen = sizeof(peername);
		if (
			getpeername(socket, (sockaddr*) &peername, &peernameLen) < 0 ||
			peernameLen != sizeof(peername) ||
			peername.sin_family != AF_INET)
		{
			abort();
		}
		//Get url to connect
		appName = (std::wstring)obj->GetProperty(L"app");
		//Get peer video capabilities
		if (obj->HasProperty(L"videoCodecs"))
			//Get value
			videoCodecs = (double)obj->GetProperty(L"videoCodecs");
		//Check if we have peer audio capabilities
		if (obj->HasProperty(L"audioCodecs"))
			//Get peer audio capabilities
			audioCodecs = (double)obj->GetProperty(L"audioCodecs");
		//Check
		if (obj->HasProperty(L"objectEncoding"))
			//Get object encoding used by client
			objectEncoding = (double)obj->GetProperty(L"objectEncoding");
		//Check fourCcList
		AMFStrictArray fourCcList;
		if (obj->HasProperty(L"fourCcList"))
		{
			//Get fourCcList supported by client
			fourCcList = static_cast<AMFStrictArray&>(obj->GetProperty(L"fourCcList"));
		}

		//Call listener
		app = listener->OnConnect(peername,appName,this,[streamId, transId, objectEncoding, selfWeak=weak_from_this()](bool accepted){
			//Log
			Log("-RTMPConnection::ProcessCommandMessage() Accepting connection [accepted:%d]\n",accepted);
			
			auto self = selfWeak.lock();
			if (!self) return;
			
			//IF not acepted
			if (!accepted)
			{
				//End connection
				self->End();
				//Done
				return;
			}
			//Send start stream
			self->SendControlMessage(RTMPMessage::UserControlMessage,RTMPUserControlMessage::CreateStreamBegin(0));
			//Send window acknoledgement
			self->SendControlMessage(RTMPMessage::WindowAcknowledgementSize, RTMPWindowAcknowledgementSize::Create(512000));
			//Send client bandwitdh
			self->SendControlMessage(RTMPMessage::SetPeerBandwidth, RTMPSetPeerBandWidth::Create(512000,2));
			//Increase chunk size
			self->maxOutChunkSize = 512;
			//Send client bandwitdh
			self->SendControlMessage(RTMPMessage::SetChunkSize, RTMPSetChunkSize::Create(self->maxOutChunkSize));

			//Create params & extra info
			AMFObject* params = new AMFObject();
			AMFObject* extra = new AMFObject();
			AMFEcmaArray* data = new AMFEcmaArray();
			//Add properties
			params->AddProperty(L"fmsVer"		,L"FMS/3,5,1,525");
			params->AddProperty(L"capabilities"	,31.0);
			params->AddProperty(L"mode"		,1.0);
			extra->AddProperty(L"level"		,L"status");
			extra->AddProperty(L"code"		,L"NetConnection.Connect.Success");
			extra->AddProperty(L"description"	,L"Connection succeded");
			extra->AddProperty(L"data"		,data);
			extra->AddProperty(L"objectEncoding"	,objectEncoding);
			data->AddProperty(L"version"           	,L"3,5,1,525");
			//Create
			self->SendCommandResult(streamId,transId,params,extra);
			//Ping
			self->PingRequest();
		});

		//If it is null
		if (!app)
			//Send error
			return SendCommandError(streamId,transId);
	} else if (name.compare(L"createStream")==0 || name.compare(L"initStream")==0) {
		//Lock mutex
		pthread_mutex_lock(&mutex);
		
		//Check if we have an application
		if (!app)
		{
			//Unlock mutex
			pthread_mutex_unlock(&mutex);
			//Send error
			return SendCommandError(streamId,transId);
		}

		//Assign the media string id
		DWORD mediaStreamId = maxStreamId++;

		//Call the application to create the stream
		auto stream = app->CreateStream(mediaStreamId,audioCodecs,videoCodecs,this);

		//Check if it was created correctly
		if (!stream)
		{
			//Unlock mutex
			pthread_mutex_unlock(&mutex);
			//Send error
			return SendCommandError(streamId,transId);
		}
		
		//Add to the streams vector
		stream->SetRTT(rtt);
		streams[mediaStreamId] = stream;
		
		//Unlock mutex
		pthread_mutex_unlock(&mutex);
		
		//Create
		SendCommandResult(streamId,transId,new AMFNull(),new AMFNumber((double)mediaStreamId));
	} else if (name.compare(L"deleteStream")==0) {
		//Check length
		if (!cmd->GetExtraLength())
		{
			//Error
			Warning("-RTMPConnection::ProcessCommandMessage() | deleteStream has no extra args\n");
			//Dump
			cmd->Dump();
			//Skip
			return;
		}
		//Get extra param
		auto extra = cmd->GetExtra(0);
		//Check type
		if (!extra->CheckType(AMFData::Number))
		{
			//Error
			Warning("-RTMPConnection::ProcessCommandMessage() | deleteStream stream id not a number\n");
			//Dump
			cmd->Dump();
			//Skip
			return;
		}
		//Get
		DWORD mediaStreamId = ((AMFNumber*)extra)->GetNumber();
		//Log
		Log("-RTMPConnection::ProcessCommandMessage() Deleting stream [%d]\n",mediaStreamId);
		
		//Lock mutex
		pthread_mutex_lock(&mutex);
		
		//Find stream
		//Check if a stream has been created with that id
		RTMPNetStreams::iterator it = streams.find(mediaStreamId);

		//If not found
		if (it==streams.end())
		{
			//Unnock mutex
			pthread_mutex_unlock(&mutex);
			//Send error
			return SendCommandError(0,transId,NULL,NULL);
		}

		//Let the application delete the stream, it will call the callback to erase it from the stream list when appropiate
		app->DeleteStream(it->second);
		
		//Unlock mutex
		pthread_mutex_unlock(&mutex);
		
		//Send eof stream
		SendControlMessage(RTMPMessage::UserControlMessage,RTMPUserControlMessage::CreateStreamEOF(mediaStreamId));
	} else if (name.compare(L"releaseStream") == 0 || name.compare(L"FCPublish") == 0) {
		//Do nothing
		SendCommandResult(streamId, transId, new AMFNull(), new AMFNull());
	} else {
		//Send
		SendCommandError(streamId,transId);
	}
}

void RTMPConnection::ProcessMediaData(DWORD streamId,RTMPMediaFrame *frame)
{
	//Check message Stream
	if (streamId)
	{
		//Lock mutex
		pthread_mutex_lock(&mutex);
		
		//Check if a stream has been created with that id
		RTMPNetStreams::iterator it = streams.find(streamId);

		//If not found
		if (it==streams.end())
		{
			//Unnock mutex
			pthread_mutex_unlock(&mutex);
			
			//Log
			Error("-RTMPConnection::ProcessMediaData() stream not found [streamId:%d]\n",streamId);
			
			//Exit (Should close connection??)
			return;
		}

		//Publish frame
		it->second->SendMediaFrame(frame);
		
		//Unnock mutex
		pthread_mutex_unlock(&mutex);
	}
}

void RTMPConnection::ProcessMetaData(DWORD streamId,RTMPMetaData *meta)
{
	Log("-ProcessMetaData [streamId:%d]\n",streamId);

	//Check message Stream
	if (streamId)
	{
		//Lock mutex
		pthread_mutex_lock(&mutex);
		
		//Check if a stream has been created with that id
		RTMPNetStreams::iterator it = streams.find(streamId);

		//If not found
		if (it==streams.end())
		{
			//Unnock mutex
			pthread_mutex_unlock(&mutex);

			//Log error
			Error("-RTMPConnection::ProcessMetaData() stream not found [streamId:%d]\n", streamId);
			
			//Exit (Should close connection??)
			return;
		}

		//Publish frame
		it->second->SendMetaData(meta);
		
		//Unnock mutex
		pthread_mutex_unlock(&mutex);
	}
}

void RTMPConnection::SendCommand(DWORD streamId,const wchar_t* name,AMFData *params,AMFData *extra)
{
	Log("-RTMPConnection::SendCommand() [streamId:%d,name:%ls]\n",streamId,name);
	//Create cmd response
	RTMPCommandMessage *cmd = new RTMPCommandMessage(name,maxTransId++,params,extra);
	//Dump
	cmd->Dump();
	//Get timestamp
	QWORD ts = getDifTime(&startTime)/1000;
	//Append message to command stream
	chunkOutputStreams[3]->SendMessage(new RTMPMessage(streamId,ts,cmd));
	//We have new data to send
	SignalWriteNeeded();
}

void RTMPConnection::SendCommandResponse(DWORD streamId,const wchar_t* name,QWORD transId,AMFData* params,AMFData *extra)
{
	Log("-RTMPConnection::SendCommandResponse() [streamId:%d,name:%ls,transId:%ld]\n",streamId,name,transId);
	//Create cmd response
	RTMPCommandMessage *cmd = new RTMPCommandMessage(name,transId,params,extra);
	//Dump
	cmd->Dump();
	//Get timestamp
	QWORD ts = getDifTime(&startTime)/1000;
	//Append message to command stream
	chunkOutputStreams[3]->SendMessage(new RTMPMessage(streamId,ts,cmd));
	//We have new data to send
	SignalWriteNeeded();
}

void RTMPConnection::SendCommandResult(DWORD streamId,QWORD transId,AMFData* params,AMFData *extra)
{
	SendCommandResponse(streamId,L"_result",transId,params,extra);
}

void RTMPConnection::SendCommandError(DWORD streamId,QWORD transId,AMFData* params,AMFData *extra)
{
	SendCommandResponse(streamId,L"_error",transId,params,extra);
}

void RTMPConnection::SendControlMessage(RTMPMessage::Type type,RTMPObject* msg)
{
	//Get timestamp
	QWORD ts = getDifTime(&startTime)/1000;
	Log("-RTMPConnection::SendControlMessage() [%s]\n",RTMPMessage::TypeToString(type));
	//Append message to control stream
	chunkOutputStreams[2]->SendMessage(new RTMPMessage(0,ts,type,msg));
	//We have new data to send
	SignalWriteNeeded();
}

void RTMPConnection::PingRequest()
{
	//Send ping request
	SendControlMessage(RTMPMessage::UserControlMessage,RTMPUserControlMessage::CreatePingRequest(getDifTime(&startTime)/1000));
}
/****************************************
 * RTMPStreamListener events
 *
 ****************************************/
void RTMPConnection::onAttached(RTMPMediaStream *stream)
{

}
void RTMPConnection::onDetached(RTMPMediaStream *stream)
{

}
void RTMPConnection::onStreamBegin(DWORD streamId)
{
	Log("-RTMPConnection::onStreamBegin() [stremId:%d]\n",streamId);
	//Send control message
	SendControlMessage(RTMPMessage::UserControlMessage,RTMPUserControlMessage::CreateStreamBegin(streamId));
}

/*void RTMPConnection::onStreamIsRecorded(DWORD streamId)
{
	//Send control message
	SendControlMessage(RTMPMessage::UserControlMessage,RTMPUserControlMessage::CreateStreamIsRecorded(streamId));
}*/

void RTMPConnection::onStreamEnd(DWORD streamId)
{
	//Send control message
	SendControlMessage(RTMPMessage::UserControlMessage,RTMPUserControlMessage::CreateStreamEOF(streamId));
}

void RTMPConnection::onCommand(DWORD streamId,const wchar_t *name,AMFData* obj)
{
	//Send new command
	SendCommand(streamId,name,new AMFNull(),obj->Clone());
}

void RTMPConnection::onNetStreamStatus(DWORD streamId,QWORD transId,const RTMPNetStatusEventInfo &info,const wchar_t *message)
{
	RTMPNetStatusEvent event(info.code,info.level,message);
	SendCommandResponse(streamId,L"onStatus",transId,new AMFNull(),event.Clone());
}

void RTMPConnection::onMediaFrame(DWORD streamId,RTMPMediaFrame *frame)
{
	//Get the timestamp from the frame
	QWORD ts = frame->GetTimestamp();

	//Check timestamp
	if (ts==(QWORD)-1)
		//Calculate timestamp based on current time
		ts = getDifTime(&startTime)/1000;

	//Dependign on the streams
	switch(frame->GetType())
	{
		case RTMPMediaFrame::Audio:
			//Append to the audio trunk
			chunkOutputStreams[4]->SendMessage(new RTMPMessage(streamId,ts,frame->Clone()));
			break;
		case RTMPMediaFrame::Video:
			chunkOutputStreams[5]->SendMessage(new RTMPMessage(streamId,ts,frame->Clone()));
			break;
	}
	//Signal frames
	SignalWriteNeeded();
}

void RTMPConnection::onMetaData(DWORD streamId,RTMPMetaData *meta)
{
	meta->Dump();
	//Get the timestamp of the metadata
	QWORD ts = meta->GetTimestamp();

	//Check timestamp
	if (ts==(QWORD)-1)
		//Calculate timestamp based on current time
		ts = getDifTime(&startTime)/1000;

	//Append to the comand trunk
	chunkOutputStreams[3]->SendMessage(new RTMPMessage(streamId,ts,meta->Clone()));
	//Signal frames
	SignalWriteNeeded();
}

void RTMPConnection::onStreamReset(DWORD id)
{
	std::vector<uint32_t> abortChunkIds;
	
	//Lock mutex
	pthread_mutex_lock(&mutex);

	for (RTMPChunkOutputStreams::iterator it=chunkOutputStreams.begin(); it!=chunkOutputStreams.end();++it)
	{
		//Get stream
		RTMPChunkOutputStream* chunkOutputStream = it->second;
		//Get chunk id
		DWORD chunkId = it->first;

		//Check it it has data pending
		if (chunkOutputStream->ResetStream(id))
		{
			abortChunkIds.push_back(chunkId);
		}
	}

	//Lock mutex
	pthread_mutex_unlock(&mutex);
	
	//Send Abort message later
	for (auto chunkId : abortChunkIds)
	{
		SendControlMessage(RTMPMessage::AbortMessage, RTMPAbortMessage::Create(chunkId));
	}

	//We need to write
	SignalWriteNeeded();
}

void RTMPConnection::onNetStreamDestroyed(DWORD streamId)
{
	Log("-RTMPConnection::onNetStreamDestroyed() Releasing stream [id:%d]\n",streamId);

	//Lock mutex
	pthread_mutex_lock(&mutex);
	
	//Find stream
	RTMPNetStreams::iterator it = streams.find(streamId);

	//If not found
	if (it!=streams.end())
		//Remove it from streams
		streams.erase(it);
	
	//Lock mutex
	pthread_mutex_unlock(&mutex);
}

void RTMPConnection::onNetConnectionStatus(QWORD transId,const RTMPNetStatusEventInfo &info,const wchar_t *message)
{
	RTMPNetStatusEvent event(info.code,info.level,message);
	//Send command
	SendCommandResponse(0,L"onStatus",transId,new AMFNull(),event.Clone());
}

void RTMPConnection::onNetConnectionDisconnected()
{
	Log("-RTMPConnection::onNetConnectionDisconnected() [%p]\n",this);

	//Close socket and event loop
	Stop();
}
