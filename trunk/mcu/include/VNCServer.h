/* 
 * File:   VNCServer.h
 * Author: Sergio
 *
 * Created on 4 de diciembre de 2013, 15:51
 */

#ifndef VNCSERVER_H
#define	VNCSERVER_H

#include <rfb/rfb.h>
#include "fifo.h"
#include "wait.h"
#include "websockets.h"
#include "use.h"

class VNCServer :
	public WebSocket::Listener
{
public:
	class Client
	{
	public:
		Client(int id,rfbScreenInfo* screen);
		virtual ~Client();
		int Connect(WebSocket* ws);
		int Process(const BYTE* data,DWORD size);
		int ProcessMessage();
		int GetId(){return id;}
		void Update();
		void ResizeScreen(rfbScreenInfo* screen);

		//Socket functionas
		 void Close();
		 int WaitForData(DWORD usecs);
		 void CancelWait();
		 int Read(char *data, int size,int timeout);
		 int Write(const char *data, int size);
	public:
		int id;
		rfbClientRec* cl;
		WebSocket* ws;
		fifo<BYTE,65535> buffer;
		Wait wait;
	};
public:
	typedef std::map<int,Client*> Clients;
	
public:
	VNCServer();
	virtual ~VNCServer();

	int Init();
	int Connect(int partId,WebSocket *socket);
	int Disconnect(WebSocket *socket);
	int SetSize(int width,int height);
	int CopyRect(BYTE *data,int src_x, int src_y, int w, int h, int dest_x, int dest_y);
	int FrameBufferUpdate(BYTE *data,int x,int y,int width,int height);
	int FrameBufferUpdateDone();
	int End();

	virtual void onOpen(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type, const DWORD length);
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size);
	virtual void onMessageEnd(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onClose(WebSocket *ws);
public:
	static void onMouseEvent(int buttonMask, int x, int y, rfbClientRec* cl);
	static void onUpdateDone(rfbClientRec* cl, int result);
private:
	rfbScreenInfo* screen;
	Clients clients;
	Use use;
};

#endif	/* VNCSERVER_H */

