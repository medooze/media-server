#ifndef _WebSocketConnection_H_
#define _WebSocketConnection_H_
#include <pthread.h>
#include <sys/poll.h>
#include <pthread.h>
#include <map>
#include <list>
#include "config.h"
#include "log.h"
#include "tools.h"
#include "fifo.h"
#include "websockets.h"
#include "http.h"
#include "httpparser.h"


class WebSocketFrameHeader
{
public:
	enum OpCode
	{
		ContinuationFrame	= 0x0,
		TextFrame		= 0x1,
		BinaryFrame		= 0x2,
		Reserved3		= 0x3,
		Reserved4		= 0x4,
		Reserved5		= 0x5,
		Reserved6		= 0x6,
		Reserved7		= 0x7,
		Close			= 0x8,
		Ping			= 0x9,
		Pong			= 0xA,
		ReservedB		= 0xB,
		ReservedC		= 0xC,
		ReservedD		= 0xD,
		ReservedE		= 0xE,
		ReservedF		= 0xF,
	};

	class Parser
	{
		public:
			Parser()
			{
				header = NULL;
				len = 0;
			}

			~Parser()
			{
				if (header) delete(header);
			}

			int Parse(BYTE* data,DWORD size)
			{
				//IF still have a parsed header
				if (header==NULL && IsParsed())
					//Do nothing
					return 0;
				//Nothing yet
				DWORD pos = 0;
				//If no header
				if (header==NULL)
				{
					//Create new
					header = new WebSocketFrameHeader();
					//No length
					len = 0;
				}
				//We need first two bytes to know size
				if (len<2)
				{
					//Get missing
					BYTE c = 2-len;
					//IF we don't have enought
					if (size<c)
						//Only what's on it
						c = size;
					//Copy next
					memcpy(header->data+len,data,c);
					//Increase len
					len += c;
					//Remove size
					size-= c;
					//Increase position
					pos += c;
				}
				//Check if we still have data and header
				if (size)
				{
					//Get missing for complete header
					BYTE c = header->GetSize()-len;
					//IF we don't have enought
					if (size<c)
						//Only what's on it
						c = size;
					//Copy next
					memcpy(header->data+len,data+pos,c);
					//Increase len
					len += c;
					//Remove size
					size-= c;
					//Increase position
					pos += c;
				}
				//Return consumed
				return pos;
			}

			bool IsParsed()
			{
				return header ? header->GetSize()==len : false;
			}

			WebSocketFrameHeader* ConsumeHeader()
			{
				//Get header
				WebSocketFrameHeader* ret = header;
				//Remvoe current header
				header = NULL;
				//Return parsed header
				return ret;
			}

		private:
			WebSocketFrameHeader* header;
			DWORD len;
	};

	friend class Parser;
public:
	WebSocketFrameHeader(bool fin,OpCode opCode,QWORD len,DWORD mask)
	{
		//Empty
		memset(data,0,14);
		//Set data
		SetFin(fin);
		SetOpCode(opCode);
		SetPayloadLength(len);
		if (mask) SetMask(mask);
	}
	/*
	      0                   1                   2                   3
	      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	     +-+-+-+-+-------+-+-------------+-------------------------------+
	     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
	     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
	     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
	     | |1|2|3|       |K|             |                               |
	     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
	     |     Extended payload length continued, if payload len == 127  |
	     + - - - - - - - - - - - - - - - +-------------------------------+
	     |                               |Masking-key, if MASK set to 1  |
	     +-------------------------------+-------------------------------+
	     | Masking-key (continued)       |          Payload Data         |
	     +-------------------------------- - - - - - - - - - - - - - - - +
	     :                     Payload Data continued ...                :
	     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
	     |                     Payload Data continued ...                |
	     +---------------------------------------------------------------+
	 */
	bool	IsFin()		{ return data[0] & 0x80;		}
	OpCode	GetOpCode()	{ return (OpCode) (data[0] & 0x0F);	}
	bool	IsMasked()	{ return data[1] & 0x80;		}
	DWORD	GetMask()	{ return IsMasked()? get4(data,1+GetPayloadLenghtSize()) : 0;		}
	BYTE*	GetData()	{ return data;								}
	DWORD	GetSize()	{ return 1 + GetPayloadLenghtSize() + IsMasked()*4;			}
	QWORD	GetPayloadLength()
	{
		QWORD len = data[1] & 0x7F;
		if (len==126)
			return get2(data,2);
		else if (len==127)
			return get8(data,2);
		return len;
	}
private:
	WebSocketFrameHeader()
	{
		//Empty
		memset(data,0,14);
	}
	void	SetFin(bool fin)
	{
		data[0] = (data[0] & 0x7F) | (fin<<7);
	}
	void	SetMask(DWORD mask)
	{
		//Set mask bit
		data[1] = data[1] | 0x80;
		//Set mask key
		set4(data,1+GetPayloadLenghtSize(),mask);
	}
	void	SetOpCode(OpCode opCode)
	{
		data[0] = (data[0] & 0x80) | opCode;
	}
	void	SetPayloadLength(QWORD len)
	{
		if (len<126)
		{
			//1 byte & len
			data[1] |= len;
		}
		else if (len<0xFFFF)
		{
			//2 bytes
			data[1] |= 126;
			//Set lenght
			set2(data,2,len);
		} else {
			//8 bytes
			data[1] |= 127;
			//Set lenght
			set8(data,2,len);
		}

	}
	BYTE GetPayloadLenghtSize()
	{
		QWORD len = data[1] & 0x7F;
		if (len==126)
			return 3;
		else if (len==127)
			return 9;
		return 1;
	}
private:
	BYTE	data[14];
};


class WebSocketConnection :
	public WebSocket,
	public HTTPParser::Listener
{
private:
	class Frame
	{
	public:
		Frame(bool fin,WebSocketFrameHeader::OpCode opCode,const BYTE* data,DWORD size)
		{
			//Store opCode
			this->opCode = opCode;
			//Create header
			WebSocketFrameHeader header(fin,opCode,size,0);
			//Get size
			headerSize = header.GetSize();
			//Calculate total size
			this->size = size+headerSize;
			//Set values
			this->data = (BYTE*)calloc(this->size,1);
			//Copy header data
			memcpy(this->data,header.GetData(),header.GetSize());
			//Set initial length
			length = header.GetSize();
			//If we have payload
			if (data)
				//Append it
				Append(data,size);
		}

		bool Append(const BYTE* data,DWORD size)
		{
			//Check
			if (size+length>this->size)
				//Error
				return Error("-WebSocketConnection::Frame not enoguth length for appending data size:%d,length:%d,data:%d",this->size,length,size);
			//Copy payload data
			memcpy(this->data+length,data,size);
			//Set length
			length += size;
		}

		~Frame()
		{
			free(data);
		}
		const WebSocketFrameHeader::OpCode GetOpCode()	{ return opCode;	}
		
		const BYTE* GetData()	{ return data;	}
		const DWORD GetSize()	{ return size;	}

		BYTE*	    GetPayloadData() { return data+headerSize;	}
		const DWORD GetPayloadSize() { return size-headerSize;	}
	private:
		WebSocketFrameHeader::OpCode opCode;
		BYTE* data;
		DWORD size;
		DWORD headerSize;
		DWORD length;
	};
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onUpgradeRequest(WebSocketConnection* conn) = 0;
		virtual void onDisconnected(WebSocketConnection* conn) = 0;
	};
public:
	WebSocketConnection(Listener* listener);
	~WebSocketConnection();

	int Init(int fd);
	int End();


	//Weksocket
	virtual void Accept(WebSocket::Listener *listener);
	virtual void Reject(const WORD code, const char* reason);
	virtual void SendMessage(MessageType type,const BYTE* data, const DWORD size);
	virtual void SendMessage(const std::wstring& message);
	virtual void SendMessage(const BYTE* data, const DWORD size);
	virtual DWORD GetWriteBufferLength();
	virtual bool IsWriteBufferEmtpy();
	virtual void Close(const WORD code, const std::wstring& reason);
	virtual void Close();
	virtual void ForceClose();
	virtual void Detach();

	//HTTPParser listener
	virtual int on_url (HTTPParser*, const char *at, DWORD length);
	virtual int on_header_field (HTTPParser*, const char *at, DWORD length);
	virtual int on_header_value (HTTPParser*, const char *at, DWORD length);
	virtual int on_body (HTTPParser*, const char *at, DWORD length);
	virtual int on_message_begin (HTTPParser*);
	virtual int on_status_complete (HTTPParser*);
	virtual int on_headers_complete (HTTPParser*);
	virtual int on_message_complete (HTTPParser*);

	HTTPRequest* GetRequest() { return request; }
protected:
	void Start();
	void Stop();
	int Run();
	Frame* GetNextFrame();
private:
	static  void* run(void *par);
	void   ProcessData(BYTE *data,DWORD size);
	int    WriteData(BYTE *data,const DWORD size);
	void   SignalWriteNeeded();
	void   Ping();
private:
	int socket;
	pollfd ufds[1];
	bool inited;
	bool running;

	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_mutex_t mutexListener;

	timeval startTime;
	Listener *listener;

	bool upgraded;
	DWORD recvSize;
	DWORD inBytes;
	DWORD outBytes;

	HTTPParser parser;
	HTTPRequest* request;
	HTTPResponse* response;
	std::string headerField;
	std::string headerValue;

	WebSocket::Listener *wsl;
	WebSocketFrameHeader::Parser headerParser;
	WebSocketFrameHeader* header;
	QWORD framePos;

	QWORD bandIni;
	DWORD bandSize;
	DWORD bandCalc;
	fifo<BYTE,65538> incoming;
	WORD		 incomingFrameLength;

	std::list<Frame*>  frames;
	DWORD		   outgoingFramesLength;
	Frame*		   pong;
};

#endif
