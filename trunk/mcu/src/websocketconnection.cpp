#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string>
#include <openssl/sha.h>
#include "http.h"
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <list>
#include "log.h"
#include "tools.h"
#include "websocketconnection.h"

WebSocketConnection::WebSocketConnection(Listener *listener)
{
	//Store listener
	this->listener = listener;
	//Not inited
	inited = false;
	running = false;
	socket = FD_INVALID;
	thread = NULL;
	//Not uypgraded yet
	upgraded = false;
	//No incoming frame yet
	incomingFrameLength = 0;
	//No request or response
	request = NULL;
	response = NULL;
	header = NULL;
	//Set initial time
	gettimeofday(&startTime,0);
	//Init mutex
	pthread_mutex_init(&mutex,0);
}

WebSocketConnection::~WebSocketConnection()
{
	//End just in case
	End();
	//Remove pending frames
	while (!frames.empty())
	{
		//Delete first frame from list
		delete(frames.front());
		//Remove from queue
		frames.pop_front();
	}
	if (request)  delete(request);
	if (response) delete(response);
	if (header)   delete(header);
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

int WebSocketConnection::Init(int fd)
{
	Log(">WebSocket Connection init [%d]\n",fd);

	//Store socket
	socket = fd;

	//I am inited
	inited = true;

	//Start parser
	parser.Init(this,HTTPParser::HTTP_REQUEST);

	//Start
	Start();

	Log("<WebSocket Connection init\n");

	return 1;
}

void WebSocketConnection::Start()
{
	//We are running
	running = true;
	
	//Create thread
	createPriorityThread(&thread,run,this,0);
}

void WebSocketConnection::Stop()
{
	//If got socket
	if (running)
	{
		//Not running;
		running = false;
		//Close socket
		shutdown(socket,SHUT_RDWR);
		//Will cause poll to return
		close(socket);
		//No socket
		socket = FD_INVALID;
	}
}

void WebSocketConnection::Close()
{
}

int WebSocketConnection::End()
{
	//Check we have been inited
	if (!inited)
		//Exit
		return 0;

	Log(">End WebSocket connection\n");

	//Not inited any more
	inited = false;

	//Stop just in case
	Stop();

	//If running
	if (thread)
	{
		//Wait for server thread to close
		pthread_join(thread,NULL);
		//No thread
		thread = NULL;
	}

	//If got listener
	if (listener)
		//Send end
		listener->onDisconnected(this);

	//Don't send more events
	listener = NULL;

	//Ended
	Log("<End WebSocket connection\n");

	return 1;
}

/***********************
* run
*       Helper thread function
************************/
void * WebSocketConnection::run(void *par)
{
        Log("-WebSocket Connecttion Thread [%d,0x%x]\n",getpid(),par);

	//Block signals to avoid exiting on SIGUSR1
	blocksignals();

        //Obtenemos el parametro
        WebSocketConnection *con = (WebSocketConnection *)par;

        //Ejecutamos
        pthread_exit((void *)con->Run());
}

/***************************
 * Run
 * 	Server running thread 
 ***************************/
int WebSocketConnection::Run()
{
	BYTE data[MTU];
	DWORD size = MTU;

	Log(">Run connection [%p]\n",this);

	//Set values for polling
	ufds[0].fd = socket;
	ufds[0].events = POLLIN | POLLERR | POLLHUP;

	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(socket,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	fcntl(socket,F_SETFL,fsflags);

	//Set no delay option
	int flag = 1;
        setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
	//Catch all IO errors
	signal(SIGIO,EmptyCatch);

	//Run until ended
	while(running)
	{
		//Wait for events
		if(poll(ufds,1,-1)<0)
			//Check again
			continue;

		if (ufds[0].revents & POLLOUT) 
		{
			//Check if we have http response
			if (response)
			{
				//Serialize
				std::string out = response->Serialize();
				//Send it
				outBytes += write(socket,out.c_str(),out.length());
				//Delete it
				delete(response);
				//Nullify
				response = NULL;
			} else {
				//Get next frame to send
				Frame* frame = GetNextFrame();
				//Check length
				if (frame)
				{
					//Send it
					outBytes += write(socket,frame->GetData(),frame->GetSize());
					//Delete it
					delete(frame);
				}
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
				ProcessData(data,len);
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

	Log("<Run WebSocket connection\n");
}

void WebSocketConnection::SignalWriteNeeded()
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

	//Check thred
	if (thread)
		//Signal the pthread this will cause the poll call to exit
		pthread_kill(thread,SIGIO);
}

WebSocketConnection::Frame* WebSocketConnection::GetNextFrame()
{
	DWORD len = 0;
	Frame* frame = NULL;

	//Lock mutex
	pthread_mutex_lock(&mutex);

	//if there are frames waiting (should always be)
	if (frames.size())
	{
		//Write next chunk from this stream
		frame = frames.front();

		//Remove it
		frames.pop_front();

		//Update length
		len = frame->GetSize();
	}
	
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
			//Calculate bandwith in kbps
			bandCalc = bandSize*8000/elapsed;
	} else {
		//Check
		if (elapsed>1000000)
		{
			//Calculate bandwith in kbps
			bandCalc = bandSize*8000/elapsed;
			//LOg
			bandIni = getDifTime(&startTime);
			bandSize = 0;
		}
	}
		
	//Un Lock mutex
	pthread_mutex_unlock(&mutex);

	//Return frame
	return frame;
}

/***********************
 * ProcessData
 * 	Process incomming data
 **********************/
void WebSocketConnection::ProcessData(BYTE *data,DWORD size)
{
	//And total size
	recvSize += size;

	if (!upgraded)
	{
		//Parse request
		parser.Execute((char*)data,size);
	} else {
		//Process all input
		while(size)
		{
			//If we still don't have header
			if (!header)
			{
				//Parse
				DWORD len = headerParser.Parse(data,size);
				//Reduce size
				size-=len;
				data+=len;
				//Check if is header parsed
				if (headerParser.IsParsed())
				{
					//Clean data sent
					framePos = 0;
					//Get new header
					header = headerParser.ConsumeHeader();
					//Check type
					switch(header->GetOpCode())
					{
						case WebSocketFrameHeader::ContinuationFrame:
							//Do nothing
							break;
						case WebSocketFrameHeader::TextFrame:
							//Start frame
							wsl->onMessageStart(this,WebSocket::Text);
							break;
						case WebSocketFrameHeader::Close:
							//End us
							End();
							break;
						case WebSocketFrameHeader::BinaryFrame:
							//Start frame
							wsl->onMessageStart(this,WebSocket::Binary);
							break;
						case WebSocketFrameHeader::Ping:
							if (header->GetPayloadLength()==0)
							{
								//Lock mutex
								pthread_mutex_lock(&mutex);
								//Push frame
								frames.push_back(new Frame(WebSocketFrameHeader::Pong,NULL,0));
								//Un Lock mutex
								pthread_mutex_unlock(&mutex);
								//We need to write data!
								SignalWriteNeeded();
							} else {
								Error("-Websocket PING with payload readed, not supported yet");
							}
							break;
						case WebSocketFrameHeader::Pong:
							break;
					}
				}
			} else {
				//Get missing
				QWORD len = header->GetPayloadLength()-framePos;
				//Check how much data do we have readed
				if (len>size)
					//Limit
					len = size;
				//Check if it is masked
				if (header->IsMasked())
				{
					BYTE mask[4];
					//Get mask
					set4(mask,0,header->GetMask());
					//For each byte
					for (int i=0;i<len;++i)
						//XOR
						data[i] = data[i] ^ mask[(framePos+i) & 0x03];
				}
				//Check type
				switch(header->GetOpCode())
				{
					case WebSocketFrameHeader::ContinuationFrame:
					case WebSocketFrameHeader::TextFrame:
					case WebSocketFrameHeader::BinaryFrame:
						//Send data
						wsl->onMessageData(this,data,len);

						break;
					case WebSocketFrameHeader::Ping:
						//We should copy data here for the PONG
						break;
				}
				//Move pos
				framePos +=len;
				//Reduce size
				size-=len;
				data+=len;
				//Check if we have ended with the frame
				if (framePos==header->GetPayloadLength())
				{
					//Check type
					switch(header->GetOpCode())
					{
						case WebSocketFrameHeader::ContinuationFrame:
						case WebSocketFrameHeader::TextFrame:
						case WebSocketFrameHeader::BinaryFrame:
							//Check if it is end frame for message
							if (header->IsFin())
								//Send data
								wsl->onMessageEnd(this);
							break;
						case WebSocketFrameHeader::Ping:
							//We should send here the PONG with data
							break;
					}
					//Delete header
					delete(header);
					//Parse new header
					header = NULL;
				}
			}
		}
	} 
}

void  WebSocketConnection::SendMessage(const std::string& message)
{
	//Create new frame
	Frame *frame = new Frame(WebSocketFrameHeader::TextFrame,(BYTE*)message.c_str(),message.length());
		
	//Lock mutex
	pthread_mutex_lock(&mutex);

	//Push frame
	frames.push_back(frame);

	//Un Lock mutex
	pthread_mutex_unlock(&mutex);

	//We need to write data!
	SignalWriteNeeded();
}

void WebSocketConnection::SendMessage(const BYTE* data, const DWORD size)
{
	//Create new frame
	Frame *frame = new Frame(WebSocketFrameHeader::BinaryFrame,data,size);

	//Lock mutex
	pthread_mutex_lock(&mutex);

	//Push frame
	frames.push_back(frame);

	//Un Lock mutex
	pthread_mutex_unlock(&mutex);

	//We need to write data!
	SignalWriteNeeded();
}

int WebSocketConnection::on_url (HTTPParser* parser, const char *at, DWORD length)
{
	//Get value
	std::string uri(at,length);
	//Get method
	std::string method(parser->GetMethodStr());
	//Create request
	request = new HTTPRequest(method,uri,parser->GetHttpMajor(),parser->GetHttpMinor());
	//OK
	return 0;
}
int WebSocketConnection::on_header_field (HTTPParser*, const char *at, DWORD length)
{
	//Get field
	headerField = std::string(at,length);
	//OK
	return 0;
}

int WebSocketConnection::on_header_value (HTTPParser*, const char *at, DWORD length)
{
	//double check
	if (!request)
		//Error
		return 1;
	//Get value
	headerValue = std::string(at,length);
	//Add to request
	request->AddHeader(headerField,headerValue);
	//OK
	return 0;
}
int WebSocketConnection::on_body (HTTPParser*, const char *at, DWORD length)
{
	//Ignore body
	return 0;
}
int WebSocketConnection::on_message_begin (HTTPParser*)
{
	//OK
	return 0;
}
int WebSocketConnection::on_status_complete (HTTPParser*)
{
	return 0;
}
int WebSocketConnection::on_headers_complete (HTTPParser*)
{
	return 0;
}
int WebSocketConnection::on_message_complete (HTTPParser*)
{
	//Debug
	Log("-Incoming websocket connection for url:%s\n",request->GetRequestURI().c_str());

	//Check listener
	if (listener)
		//Send event
		listener->onUpgradeRequest(this);
}

void WebSocketConnection::Accept(WebSocket::Listener *wsl)
{
	//Store websocket listener
	this->wsl = wsl;
	
	/*
	If the response lacks a |Sec-WebSocket-Accept| header field or
	the |Sec-WebSocket-Accept| contains a value other than the
	base64-encoded SHA-1 of the concatenation of the |Sec-WebSocket-
	Key| (as a string, not base64-decoded) with the string "258EAFA5-
	E914-47DA-95CA-C5AB0DC85B11" but ignoring any leading and
	trailing whitespace, the client MUST _Fail the WebSocket
	Connection_.
	 */
	//Get Sec-WebSocket-Key header value
	std::string secWebSocketKey = request->GetHeader("Sec-WebSocket-Key");

	//If not found
	if (secWebSocketKey.size()==0)
	{
		//Update
		response = new HTTPResponse(400,"Bad request, no Sec-WebSocket-Key",1,1);
		//Signal write needed
		SignalWriteNeeded();
		//End
		End();
	}
	//Append
	secWebSocketKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	// response
	BYTE secWebSocketAccept[SHA_DIGEST_LENGTH];
	char secWebSocketAccept64[SHA_DIGEST_LENGTH*2];
	//SHA1 response
	SHA1((unsigned char*)secWebSocketKey.c_str(),secWebSocketKey.length(), secWebSocketAccept);
	//Calculate base 64
	av_base64_encode(secWebSocketAccept64,SHA_DIGEST_LENGTH*2,secWebSocketAccept,SHA_DIGEST_LENGTH);

	//Update
	response = new HTTPResponse(101,"Switching Protocols",1,1);
	//Add headers
	response->AddHeader("Upgrade"			, "Websocket");
	response->AddHeader("Connection"		, "Upgrade");
	//Check if we have input protocols
	if (request->HasHeader("Sec-WebSocket-Protocol"))
		//Add websockets protocols back
		response->AddHeader("Sec-WebSocket-Protocol"	, request->GetHeader("Sec-WebSocket-Protocol"));
	//Add accept key
	response->AddHeader("Sec-WebSocket-Accept"	, secWebSocketAccept64);

	//Signal write needed
	SignalWriteNeeded();
	//We are upgraded
	upgraded = true;
}

void WebSocketConnection::Reject(const WORD code, const char* reason)
{
	//Update
	response = new HTTPResponse(code,reason,1,1);
	//Signal write needed
	SignalWriteNeeded();
	//End
	End();
}
