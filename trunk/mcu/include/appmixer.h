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
#include "wait.h"
#include "VNCViewer.h"
#include "VNCServer.h"
#include "overlay.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

class AppMixer : 
	public WebSocket::Listener,
	public VNCViewer::Listener,
	public VNCServer::Listener
{
private:

	class Presenter : 
		public VNCViewer::Socket,
		public VNCViewer
	{
	public:
		Presenter(WebSocket *ws,const std::wstring &name);
		virtual ~Presenter();
		int Init(VNCViewer::Listener *listener);
		int Process(const BYTE* data,DWORD size);
		int End();

		// VNCViewer::Socket methods
		virtual int WaitForMessage(DWORD usecs);
		virtual void CancelWaitForMessage();
		virtual bool ReadFromRFBServer(char *data, unsigned int size);
		virtual bool WriteToRFBServer(char *data, int size);
		virtual void Close();
	public:
		WebSocket* ws;
		fifo<BYTE,512*1024> buffer;
		Wait wait;
		VNCViewer viewer;
		std::wstring name;
	};

	
public:
	AppMixer();
	~AppMixer();

	int Init(VideoOutput *output);
	int DisplayImage(const char* filename);
	int WebsocketConnectRequest(int partId,const std::wstring &name,WebSocket *ws,bool isPresenter);
	int SetPresenter(int partId);
	int SetEditor(int partId);
	int SetViewer(int partId);
	int GetPresenter()	{ return presenterId;	}
	int GetEditor()		{ return editorId;	}
	int End();

	virtual void onOpen(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type, const DWORD length);
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size);
	virtual void onMessageEnd(WebSocket *ws);
	virtual void onWriteBufferEmpty(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onClose(WebSocket *ws);


        virtual int onSharingStarted(VNCViewer *viewer);
	virtual int onFrameBufferSizeChanged(VNCViewer *viewer, int width, int height);
	virtual int onFrameBufferUpdate(VNCViewer *viewer,int x, int y, int w, int h);
	virtual int onGotCopyRectangle(VNCViewer *viewer, int src_x, int src_y, int w, int h, int dest_x, int dest_y);
	virtual int onFinishedFrameBufferUpdate(VNCViewer *viewer);
	virtual int onHandleCursorPos(VNCViewer *viewer,int x, int y);
	virtual int onSharingEnded(VNCViewer *viewer);

	virtual void onMouseEvent(int buttonMask, int x, int y);
	virtual void onKeyboardEvent(bool down, DWORD keySym);
private:
	Logo		logo;
	VideoOutput*	output;
	Presenter*	presenter;
	VNCServer	server;
	Use		use;
	SwsContext*	sws;
	BYTE*		img;
	DWORD		width;
	DWORD		height;
	int		presenterId;
	int		editorId;
	Canvas		*canvas;
	DWORD		lastX;
	DWORD		lastY;
};

#endif	/* APPMIXER_H */
