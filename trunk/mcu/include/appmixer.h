/* 
 * File:   appmixer.h
 * Author: Sergio
 *
 * Created on 15 de enero de 2013, 15:38
 */

#ifndef APPMIXER_H
#define	APPMIXER_H
#include <string>
#include "config.h"
#include "tools.h"
#include "video.h"
#include "logo.h"
#include "websockets.h"
#include "fifo.h"
#include "use.h"

class AppMixer : public WebSocket::Listener
{
private:
	enum ClientToServerMessage
	{
		SetPixelFormat = 0,
		SetEncodings = 2,
		FramebufferUpdateRequest = 3,
		KeyEvent = 4,
		PointerEvent = 5,
		ClientCutText = 6
	};
	
	enum ServerToClientMessage {
		FramebufferUpdate = 0,
		SetColourMapEntries = 1,
		Bell = 2,
		ServerCutText = 3
	};
	
	class Presenter
	{
	public:
		enum State { WaitingVersion,WaitingSecurity,WaitingSecurityResult,WaitingServerIni,Established,Error};
	public:
		Presenter(WebSocket *ws);
		int Process(const BYTE* data,DWORD size);
		void SendMessage(const BYTE* data,DWORD size);
		
		int CheckState(State state)	{ return this->state==state;		}
		BYTE* GetServerIniData()	{ return (BYTE*)serverIni;		}
		DWORD GetServerIniSize()	{ return sizeof(serverIni);		}
		BYTE* GetMessage()		{ return (BYTE*)message;		}
		DWORD GetLength()		{ return length;			}
		void  ClearMessage()		{ length = 0;				}
		bool  IsFramebufferUpdate()	{ return length>3 && get3(message,0)==0; 	}
	public:
		WebSocket* ws;
		BYTE serverIni[27];
		fifo<BYTE,1024> buffer;
		BYTE	message[65535];
		DWORD	length;
		State	state;
	};

	class Viewer
	{
	public:
		enum State { None,VersionSent,SecuritySent,SecurityResultSent,WaitingForServerIni,Established};
	public:
		Viewer(int id,WebSocket *ws);
		void StartHandshake();
		void SendMessage(const BYTE* data,DWORD size);
		void SendServerIni(BYTE* data,DWORD size);
		int Process(const BYTE* data,DWORD size);
		int CheckState(State state)	{ return this->state==state;	}
		BYTE* GetMessage()		{ return (BYTE*)message;	}
		DWORD GetLength()		{ return length;		}
		void  ClearMessage()		{ length = 0;			}
		bool  IsSafe()			{ return safe;			}
		void  SetSafe()			{ safe = 1;			}
		int   GetId()			{ return id;			}
	public:
		int id;
		bool safe;
		WebSocket* ws;
		State state;
		fifo<BYTE,1024> buffer;
		BYTE	message[65535];
		DWORD	length;
	};
public:
	AppMixer();

	int Init(VideoOutput *output);
	int DisplayImage(const char* filename);
	int WebsocketConnectRequest(int partId,WebSocket *ws,bool isPresenter);
	int End();

	virtual void onOpen(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type);
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size);
	virtual void onMessageEnd(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onClose(WebSocket *ws);
private:
	typedef std::map<int,Viewer*> Viewers;
private:
	Logo		logo;
	VideoOutput*	output;
	Presenter*	presenter;
	Viewers		viewers;
	Use		use;
};

#endif	/* APPMIXER_H */

