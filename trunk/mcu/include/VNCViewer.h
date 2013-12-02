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
	};

	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual int onFrameBufferSizeChanged(VNCViewer *viewer, int width, int height) = 0;
		virtual int onFrameBufferUpdate(VNCViewer *viewer,int x, int y, int w, int h) = 0;
		virtual int onFinishedFrameBufferUpdate(VNCViewer *viewer) = 0;
	};
public:
	VNCViewer();
	~VNCViewer();

	int Init(Socket *socket,Listener *listener);
	int End();

	BYTE* GetFrameBuffer()	{ return client->frameBuffer;	}
	DWORD GetWidth()	{ return client->width;		}
	DWORD GetHeight()	{ return client->height;	}
protected:
	int ResetSize();
public:
	static rfbBool MallocFrameBuffer(rfbClient* client);
	static void GotFrameBufferUpdate(rfbClient* client, int x, int y, int w, int h);
	static void FinishedFrameBufferUpdate(rfbClient* client);
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

