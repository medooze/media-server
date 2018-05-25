#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include "tools.h"
#include "rtmp/rtmphandshake.h"
#include <pthread.h>
#include <sys/poll.h>
#include "config.h"
#include "log.h"
#include "rtmp/rtmp.h"
#include "rtmp/rtmpchunk.h"
#include "rtmp/rtmpmessage.h"
#include "rtmp/rtmpstream.h"
#include "rtmp/rtmpapplication.h"

typedef std::map<DWORD,RTMPChunkInputStream*>  RTMPChunkInputStreams;
typedef std::map<DWORD,RTMPChunkOutputStream*> RTMPChunkOutputStreams;
enum State {HEADER_C0_WAIT=0,HEADER_C1_WAIT=1,HEADER_C2_WAIT=2,CHUNK_HEADER_WAIT=3,CHUNK_TYPE_WAIT=4,CHUNK_EXT_TIMESTAMP_WAIT=5,CHUNK_DATA_WAIT=6};


int main(int argc,char **argv)
{
	int inited;
	State state;

	RTMPHandshake01 s01;
	RTMPHandshake0 s0;
	RTMPHandshake0 c0;
	RTMPHandshake1 s1;
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

	pthread_t thread;
	pthread_mutex_t mutex;

	DWORD maxStreamId;
	DWORD maxTransId;

	timeval startTime;

	//Set initial state
	state = HEADER_C0_WAIT;
	//Set chunk size
	maxChunkSize = 128;
	//Set first media id
	maxStreamId = 3;

	//Check fd
	if (argc<2)
		return Error("Usage:\n\trmptdebug file\n");

	//Open file
	int fd = open(argv[1],O_RDONLY);

	//Check fd
	if (fd==-1)
		return Error("Could not open file [%s]\n",argv[1]);	

	RTMPChunkInputStreams::iterator it;
	int len;

	BYTE data[4096];
	DWORD size;
	
	while((size=read(fd,data,4096))>0)
	{
		//Get pointer and data size
		BYTE *buffer = data;
		DWORD bufferSize = size;
		BYTE *i = data;

		//While there is data
		while(bufferSize>0)
		{
			//Store previous buffer pos
			i = buffer;
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
						//Dump
						c0.Dump();
						//Depending on the client version
						if (c0.GetRTMPVersion()!=3)
							throw std::runtime_error("Unsupported Version\n");
						//Move to next state
						state = HEADER_C1_WAIT;
						//Debug
						Debug("Received c0 version: %d\n",c0.GetRTMPVersion());
					} else
						return Error("Header C0 not parsed correctly");
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
						//Dump
						c1.Dump();
						Debug("-Received C1 client version [%d,%d,%d,%d]\n",c1.GetVersion()[0],c1.GetVersion()[1],c1.GetVersion()[2],c1.GetVersion()[2]);
						//Move to next state
						state = HEADER_C2_WAIT;
					} else
						return Error("Header C1 not parsed correctly");
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
						c2.Dump();
						//Move to next state
						state = CHUNK_HEADER_WAIT;
						//Debug
						Debug("Received c2, sending c2. CONNECTED.\n");
					} else
						return Error("Header C2 not parsed correctly");
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
						//Dump header
						header.Dump();
						//Clean all buffers
						type0.Reset();
						type1.Reset();
						type2.Reset();
						extts.Reset();
						//Move to next state
						state = CHUNK_TYPE_WAIT;
					} else
						return Error("Error parsing chunk header");

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
						Debug("Creating new chunk stream [id:%d]\n",chunkStreamId);
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
								//Dump
								type0.Dump();
								//Set data for stream
								chunkInputStream->SetMessageLength	(type0.GetMessageLength());
								chunkInputStream->SetMessageTypeId	(type0.GetMessageTypeId());
								chunkInputStream->SetMessageStreamId	(type0.GetMessageStreamId());
								//Check if we have extended timestamp
								if (type0.GetTimestamp()!=0xFFFFFF)
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
								//Dump
								type1.Dump();
								//Set data for stream
								chunkInputStream->SetMessageLength(type1.GetMessageLength());
								chunkInputStream->SetMessageTypeId(type1.GetMessageTypeId());
								//Check if we have extended timestam
								if (type1.GetTimestampDelta()!=0xFFFFFF)
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
								//Dump
								type2.Dump();
								//Check if we have extended timestam
								if (type2.GetTimestampDelta()!=0xFFFFFF)
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
							Debug("RTMPChunkType3\n");
							//No header chunck
							len = 0;
							//If it is the first chunk
							if (chunkInputStream->IsFirstChunk())
								//Increase timestamp with previous delta
								chunkInputStream->IncreaseTimestampWithDelta();
							//Start data reception
							chunkInputStream->StartChunkData();
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
					len = extts.Parse(buffer,bufferSize);
					//Move
					buffer+=len;
					bufferSize-=len;
					//If it is parsed
					if (extts.IsParsed())
					{
						extts.Dump();
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
						//read more
						continue;
					//Parse data
					len = chunkInputStream->Parse(buffer,len);
					//Check if it has parsed a msg
					if (chunkInputStream->IsParsed())
					{
						//Get message
						RTMPMessage* msg = chunkInputStream->GetMessage();
						//Dump
						msg->Dump();
						//Get message type
						BYTE type = msg->GetType();
						//Check message type is set chunk size
						if (msg->IsControlProtocolMessage() && type==RTMPMessage::SetChunkSize)
						{
							//Get control set chunk size protocl message
							RTMPSetChunkSize *ctrl = (RTMPSetChunkSize *)msg->GetControlProtocolMessage();
							//Get new chunk size
							maxChunkSize = (ctrl)->GetChunkSize();
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
					break;
			}
		}
	}
	return 0;
}

