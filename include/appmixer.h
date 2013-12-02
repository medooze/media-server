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
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

class AppMixer : 
	public WebSocket::Listener,
	public VNCViewer::Listener
{
private:

	class Presenter : 
		public VNCViewer::Socket
	{
	public:
		Presenter(WebSocket *ws);
		~Presenter();
		int Init(VNCViewer::Listener *listener);
		int Process(const BYTE* data,DWORD size);
		int End();

		// VNCViewer::Socket listeners
		virtual int WaitForMessage(DWORD usecs);
		virtual void CancelWaitForMessage();
		virtual bool ReadFromRFBServer(char *data, unsigned int size);
		virtual bool WriteToRFBServer(char *data, int size);
	public:
		WebSocket* ws;
		fifo<BYTE,65535> buffer;
		Wait wait;
		VNCViewer viewer;
	};

	class Viewer
	{
	public:
		Viewer(int id,WebSocket *ws);
		int Process(const BYTE* data,DWORD size);
		void  SetSafe()			{ safe = 1;			}
		int   GetId()			{ return id;			}
	public:
		int id;
		bool safe;
		WebSocket* ws;
		fifo<BYTE,65535> buffer;
		Wait wait;
	};
public:
	AppMixer();
	~AppMixer();

	int Init(VideoOutput *output);
	int DisplayImage(const char* filename);
	int WebsocketConnectRequest(int partId,WebSocket *ws,bool isPresenter);
	int End();

	virtual void onOpen(WebSocket *ws);
	virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type, const DWORD length);
	virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size);
	virtual void onMessageEnd(WebSocket *ws);
	virtual void onError(WebSocket *ws);
	virtual void onClose(WebSocket *ws);

	virtual int onFrameBufferSizeChanged(VNCViewer *viewer, int width, int height);
	virtual int onFrameBufferUpdate(VNCViewer *viewer,int x, int y, int w, int h);
	virtual int onFinishedFrameBufferUpdate(VNCViewer *viewer);
private:
	typedef std::map<int,Viewer*> Viewers;
private:
	Logo		logo;
	VideoOutput*	output;
	Presenter*	presenter;
	Viewers		viewers;
	Use		use;
	SwsContext*	sws;
	BYTE*		img;
	DWORD		width;
	DWORD		height;
};

#endif	/* APPMIXER_H */

