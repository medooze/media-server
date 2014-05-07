/* 
 * File:   VNCViewer.h
 * Author: Sergio
 *
 * Created on 15 de noviembre de 2013, 12:37
 */

#ifndef VNCVIEWER_H
#define	VNCVIEWER_H
#include "config.h"
#include "rfb/rfbclient.h"

class VNCViewer
{
public:
	class Socket
	{
	public:
		//Virtual desctructor
		virtual ~Socket(){};
	public:
		//Interface
		virtual void CancelWaitForMessage() = 0;
		virtual int WaitForMessage(DWORD usecs) = 0;
		virtual bool ReadFromRFBServer(char *out, unsigned int n) = 0;
		virtual bool WriteToRFBServer(char *buf, int n) = 0;
		virtual void Close() = 0;
	};

	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual int onSharingStarted(VNCViewer *viewer) = 0;
		virtual int onFrameBufferSizeChanged(VNCViewer *viewer, int width, int height) = 0;
		virtual int onFrameBufferUpdate(VNCViewer *viewer,int x, int y, int w, int h) = 0;
		virtual int onGotCopyRectangle(VNCViewer *viewer, int src_x, int src_y, int w, int h, int dest_x, int dest_y) = 0;
		virtual int onFinishedFrameBufferUpdate(VNCViewer *viewer) = 0;
		virtual int onHandleCursorPos(VNCViewer *viewer,int x, int y) = 0;
		virtual int onSharingEnded(VNCViewer *viewer) = 0;
	};
public:
	VNCViewer();
	virtual ~VNCViewer();

	int Init(Socket *socket,Listener *listener);
	int End();

	BYTE* GetFrameBuffer()	{ return client->frameBuffer;	}
	DWORD GetWidth()	{ return client->width;		}
	DWORD GetHeight()	{ return client->height;	}

	int SendMouseEvent(int buttonMask, int x, int y);
	int SendKeyboardEvent(bool down, DWORD key);
protected:
	int ResetSize();
public:
	static rfbBool MallocFrameBuffer(rfbClient* client);
	static void GotFrameBufferUpdate(rfbClient* client, int x, int y, int w, int h);
	static void GotCopyRectangle(rfbClient* client, int src_x, int src_y, int w, int h, int dest_x, int dest_y);
	static void FinishedFrameBufferUpdate(rfbClient* client);
	static rfbBool HandleCursorPos(rfbClient* client, int x, int y);
private:
	static void * run(void *par);
protected:
	int Run();
private:
	Listener* listener;
	rfbClient* client;
	pthread_t  thread;
	bool	   running;
};

#endif	/* VNCVIEWER_H */

