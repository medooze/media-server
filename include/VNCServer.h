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
		Client(int id,const std::wstring &name,VNCServer* server);
		virtual ~Client();
		int Connect(WebSocket* ws);
		int Disconnect();
		int Process(const BYTE* data,DWORD size);
		int ProcessMessage();
		int GetId(){return id;}
		void Update();
		void FreezeUpdate(bool freeze);
		void ResizeScreen();
		void Reset();
		void SetViewOnly(bool viewOnly);
		VNCServer* GetServer() { return server; }
		std::wstring GetName() { return name;	}
		
		//Socket functionas
		 void Close();
		 int WaitForData(DWORD usecs);
		 int Read(char *data, int size,int timeout);
		 int Write(const char *data, int size);
	protected:
		int Run();

	private:
		static void *run(void *par);
	private:
		int id;
		VNCServer* server;
		rfbClientRec* cl;
		WebSocket* ws;
		fifo<BYTE,512*1024> buffer;
		Wait wait;
		int reset;
		int freeze;
		pthread_t thread;
		std::wstring name;
	};

	class Listener
	{
	public:
		virtual void onMouseEvent(int buttonMask, int x, int y) = 0;
		virtual void onKeyboardEvent(bool down, DWORD keySym) = 0;
	};
public:
	VNCServer();
	virtual ~VNCServer();

	int Init(Listener* listener);
	int SetEditor(int editorId);
	int SetViewer(int viewerId);
	std::wstring GetEditorName();
	int Connect(int partId,const std::wstring &name,WebSocket *socket,const std::string &to);
	int Disconnect(WebSocket *socket);
	int Reset();
	int SetSize(int width,int height);
	int CopyRect(BYTE *data,int src_x, int src_y, int w, int h, int dest_x, int dest_y);
	int FrameBufferUpdate(const BYTE *data,int srcX,int srcY,int srcLineSize,int x,int y,int width,int height);
	int FrameBufferUpdateDone();
	int GetWidth() { return screen ? screen->width : 0; }
	int GetHeight() { return screen ? screen->height : 0; }
	int End();

	virtual void onOpen(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type, const DWORD length);
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size);
	virtual void onMessageEnd(WebSocket *ws);
	virtual void onWriteBufferEmpty(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onClose(WebSocket *ws);

	rfbScreenInfo* GetScreenInfo()	{ return screen;	}
	
public:
	static void onMouseEvent(int buttonMask, int x, int y, rfbClientRec* cl);
	static void onKeyboardEvent(rfbBool down, rfbKeySym keySym, rfbClientRec* cl);
	static void onUpdateDone(rfbClientRec* cl, int result);
	static void onDisplay(rfbClientRec* cl);
	static void onDisplayFinished(rfbClientRec* cl, int result);

protected:
	Listener* listener;
	int editorId;
private:
	typedef std::map<int,Client*> Clients;
private:
	rfbScreenInfo* screen;
	Clients clients;
	Use use;
	int viewerId;
};

#endif	/* VNCSERVER_H */

