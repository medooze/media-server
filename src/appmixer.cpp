/*
 * File:   appmixer.cpp
 * Author: Sergio
 *
 * Created on 15 de enero de 2013, 15:38
 */

#include "appmixer.h"
#include "log.h"
#include "fifo.h"
#include "use.h"
#include "overlay.h"


AppMixer::AppMixer()
{
	//No output
	output = NULL;
	canvas = NULL;
	//No presenter
	presenter = NULL;
	presenterId = 0;
	//NO editor
	editorId = 0;
	lastX = 0;
	lastY = 0;
	//No img
	img = NULL;
	// Create YUV rescaller cotext
	sws = sws_alloc_context();
}
AppMixer::~AppMixer()
{
	//End just in case
	End();
	//Clean mem
	if (img) free(img);
	//If got canvas
	if (canvas)
		//Delete it
		delete(canvas);
	//Free sws contes
	sws_freeContext(sws);
}

int AppMixer::Init(VideoOutput* output)
{
	//Set output
	this->output = output;
	//Init VNC server
	server.Init(this);
}

int AppMixer::DisplayImage(const char* filename)
{
	Log("-DisplayImage [\"%s\"]\n",filename);

	//Load image
	if (!logo.Load(filename))
		//Error
		return Error("-Error loading file");

	//Check output
	if (!output)
		//Error
		return Error("-No output");

	//Set size
	output->SetVideoSize(logo.GetWidth(),logo.GetHeight());
	//Set image
	output->NextFrame(logo.GetFrame());

	//Set size to the vnc
	server.SetSize(logo.GetWidth(),logo.GetHeight());
	//Set new frame
	server.FrameBufferUpdate(logo.GetFrameRGBA(),0,0,logo.GetWidth(),logo.GetHeight());

	//Everything ok
	return true;
}

int AppMixer::End()
{
	Log(">AppMixer::End\n");

	//Reset output
	output = NULL;

	//LOck
	use.WaitUnusedAndLock();

	//Check if presenting
	if (presenter)
	{
		//ENd presenter
		presenter->End();
		//Delete presenter
		delete(presenter);
		//Remove presneter
		presenter = NULL;
	}
	//Unlock
	use.Unlock();

	//End VNC server
	server.End();

	Log("<AppMixer::End\n");
}

int AppMixer::WebsocketConnectRequest(int partId,const std::wstring &name,WebSocket *ws,bool isPresenter)
{
	Log("-WebsocketConnectRequest [partId:%d,isPresenter:%d]\n",partId,isPresenter);

	//Check if it is a viewer
	if (!isPresenter)
		//Let the server handle it
		return server.Connect(partId,name,ws);

	//Check if we have presenter
	if (presenterId!=partId)
	{
		//Reject it
		ws->Reject(403,"Forbidden");
		//Error
		return Error("-Participant not allowed to present\n");
	}

	//LOck
	use.WaitUnusedAndLock();

	//Check if already was presenting
	if (presenter)
	{
		//End presenter
		presenter->End();
		//Delete it
		delete(presenter);
	}

	//Create new presenter
	presenter = new Presenter(ws,name);
	//Init it
	presenter->Init(this);
	
	//Unlock
	use.Unlock();
	
	//Accept connection
	ws->Accept(this);

	//Ok
	return 1;
}

int AppMixer::SetPresenter(int partId)
{
	//Log
	Log(">AppMixer::SetPresenter [%d]\n",partId);

	//LOck
	use.WaitUnusedAndLock();
	
	//Store it
	presenterId = partId;
	
	//Check if already presenting
	if (presenter)
	{
		//End it
		presenter->End();
		//Delete it
		delete(presenter);
		//NULL
		presenter = NULL;
	}

	//Reset size
	server.Reset();
	
	//Unlock
	use.Unlock();

	//Log
	Log("<AppMixer::SetPresenter [%d]\n",partId);

	//OK
	return 1;
}

int AppMixer::SetEditor(int partId)
{
	Log(">AppMixer::SetEditor [%d]\n",partId);

	//Store it
	editorId = partId;

	//Set it to the vnc server
	server.SetEditor(editorId);
	
	//Reset coordinates from mouse
	lastX = 0;
	lastY = 0;

	Log("<AppMixer::SetEditor [%d]\n",partId);
	
	//OK
	return 1;
}

void AppMixer::onOpen(WebSocket *ws)
{
	Log("-AppMixer::onOpen %p",ws);
}

void AppMixer::onMessageStart(WebSocket *ws,const WebSocket::MessageType type,const DWORD length)
{
	//Nothing
}

void AppMixer::onMessageData(WebSocket *ws,const BYTE* data, const DWORD size)
{
	//Get ws presenter
	Presenter *wsp = (Presenter*) ws->GetUserData();

	//Ensure it is still the presenter
	if (wsp)
		//Process it
		wsp->Process(data,size);
}

void AppMixer::onMessageEnd(WebSocket *ws)
{
	//Nothing
}

void AppMixer::onWriteBufferEmpty(WebSocket *ws)
{
	//Nothing
}

void AppMixer::onClose(WebSocket *ws)
{
	//Log
	Log(">AppMixer::onClose %p %p\n",ws,presenter);

	//Get ws presenter
	Presenter *wsp = (Presenter*) ws->GetUserData();

	//If it was still attached
	if (wsp)
	{
		//Lock
		use.WaitUnusedAndLock();

		//If it was the current presenter
		if (wsp==presenter)
		{
			//Remove presenter
			presenter = NULL;
			//Reset server
			server.Reset();
		}

		//Unlock
		use.Unlock();
		
		//End it
		wsp->End();
		//Delete it
		delete(wsp);
	}

	//Log
	Log("<AppMixer::onClose\n");
}

void AppMixer::onError(WebSocket *ws)
{
	Log(">AppMixer::onError %p %p\n",ws,presenter);

	//Get ws presenter
	Presenter *wsp = (Presenter*) ws->GetUserData();

	//If it was still attached
	if (wsp)
	{
		//Lock
		use.WaitUnusedAndLock();

		//If it was the current presenter
		if (wsp==presenter)
		{
			//Remove presenter
			presenter = NULL;
			//Reset server
			server.Reset();
		}

		//Unlock
		use.Unlock();

		//End it
		wsp->End();
		//Delete it
		delete(wsp);
	}

	Log("<AppMixer::onError\n");
}


AppMixer::Presenter::Presenter(WebSocket *ws,const std::wstring &name)
{
	//Store ws and name
	this->ws = ws;
	this->name.assign(name);
}

AppMixer::Presenter::~Presenter()
{
	//End just in case
	End();
}

int AppMixer::Presenter::Init(VNCViewer::Listener *listener)
{
	//Set presenter as data
	ws->SetUserData(this);
	//Init viewer
	return VNCViewer::Init(this,listener);
}

int AppMixer::Presenter::End()
{
	//Close websocket and stop listening for events, do it first or VNCViewer::End may dead block
	if (ws) 
	{
		//Remove us from it
		ws->SetUserData(NULL);
		//Close inmediatelly
		ws->ForceClose();
	}
	//End viewer
	VNCViewer::End();
	//NO websocket anymore
	ws = NULL; 
	//OK
	return 1;
}

int AppMixer::Presenter::Process(const BYTE* data,DWORD size)
{
	Debug("-Process size:%d\n",size);
	//Lock
	wait.Lock();
	//Puss to buffer
	if(!buffer.push(data,size))
		//Print error
		Error("-AppMixer::Presenter::Process not enought free space length:%d size:%d\n",buffer.length(),size);
	//Lock
	wait.Unlock();
	//Signal
	wait.Signal();
	//Return written
	return size;
}

int AppMixer::Presenter::WaitForMessage(DWORD usecs)
{
	Debug(">WaitForMessage %d\n",buffer.length());

	//Wait for data
	int ret = wait.WaitSignal(usecs*1000);

	Debug("<WaitForMessage %d %d\n",buffer.length(),ret);

	return ret;
}

void AppMixer::Presenter::CancelWaitForMessage()
{
	//Log
	Debug(">AppMixer::Presenter::CancelWaitForMessage\n");
	//Cancel wait
	wait.Cancel();
	//Log
	Debug("<AppMixer::Presenter::CancelWaitForMessage\n");
}

bool  AppMixer::Presenter::ReadFromRFBServer(char *out, unsigned int size)
{
	int num = 0;

	//Debug(">ReadFromRFBServer requested:%d,buffered:%d\n",size,buffer.length());

	//Lock buffer access
	wait.Lock();

	//Check if we have enought data
	while(buffer.length()<size)
	{
		//Wait for mor data
		if (!wait.PreLockedWaitSignal(0))
			//Error
			goto end;
	}

	//Read
	num = buffer.pop((BYTE*)out,size);

end:
	//Unlock
	wait.Unlock();

	//Debug("<ReadFromRFBServer requested:%d,returned:%d,left:%d\n",size,num,buffer.length());

	//Return readed
	return num;
}
bool AppMixer::Presenter::WriteToRFBServer(char *buf, int size)
{
	//Send data
	ws->SendMessage((BYTE*)buf,size);
	//OK
	return size;
}

void AppMixer::Presenter::Close()
{
	//Close use
	ws->Close();
}

int AppMixer::onSharingStarted(VNCViewer *viewer)
{
	//Nothing
	Log("-onSharingStarted\n");
}

int AppMixer::onSharingEnded(VNCViewer *viewer)
{
	//Log
	Log("-onSharingEnded\n");
	//End sharing
	server.Reset();
}

int AppMixer::onFrameBufferSizeChanged(VNCViewer *viewer, int width, int height)
{
	Log("-onFrameBufferSizeChanged [%d,%d]\n",width,height);
	//Change server size
	server.SetSize(width,height);

	//Check if we already have a context
	if (sws)
	{
		//Free sws contes
		sws_freeContext(sws);
		//Null it
		sws = NULL;
	}

	//If got size
	if (width && height)
	{
		// Create YUV rescaller cotext
		sws = sws_alloc_context();

		// Set property's of YUV rescaller context
		av_opt_set_defaults(sws);
		av_opt_set_int(sws, "srcw",       width			,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "srch",       height		,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "src_format", AV_PIX_FMT_RGBA	,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "dstw",       width			,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "dsth",       height		,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "dst_format", PIX_FMT_YUV420P	,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "sws_flags",  SWS_FAST_BILINEAR	,AV_OPT_SEARCH_CHILDREN);
	
		// Init YUV rescaller context
		if (sws_init_context(sws, NULL, NULL) < 0)
			//Set errror
			return  Error("Couldn't init sws context\n");
	}

	//If already got memory
	if (img)
		//Free it
		free(img);

	//Create number of pixels
	DWORD num = width*height;
	//Get size with padding
	DWORD size = (((width/32+1)*32)*((height/32+1)*32)*3)/2+FF_INPUT_BUFFER_PADDING_SIZE+32;
	//Allocate memory
	img = (BYTE*) malloc32(size);

	// paint the background in black for YUV
	memset(img	, 0		, num);
	memset(img+num	, (BYTE) -128	, num/2);

	//Set size
	if (output) output->SetVideoSize(width,height);
	
	//If got canvas
	if (canvas)
		//Delete it
		delete(canvas);
	
	//And create a new one
	canvas = new Canvas(width,height);

	return 1;
}

int AppMixer::onFrameBufferUpdate(VNCViewer *viewer,int x, int y, int w, int h)
{
	//Log("-onFrameBufferUpdate [%d,%d,%d,%d]\n",x,y,w,h);

	//UPdate vnc server frame buffer
	server.FrameBufferUpdate(viewer->GetFrameBuffer(),x,y,w,h);

	return 1;
}

int AppMixer::onGotCopyRectangle(VNCViewer *viewer, int src_x, int src_y, int w, int h, int dest_x, int dest_y)
{
	//Copy vnc server frame buffer rect to destination
	server.CopyRect(viewer->GetFrameBuffer(),src_x, src_y, w, h, dest_x, dest_y);

	return 1;
}

int AppMixer::onFinishedFrameBufferUpdate(VNCViewer *viewer)
{
	//Signal server
	server.FrameBufferUpdateDone();

	if (output)
	{
		DWORD width = viewer->GetWidth();
		DWORD height = viewer->GetHeight();

		//Calc num pixels
		DWORD numpixels = width*height;

		//Alloc data
		AVFrame* in = av_frame_alloc();
		AVFrame* out = av_frame_alloc();

		//Set in frame
		in->data[0] = viewer->GetFrameBuffer();

		//Set size
		in->linesize[0] = width*4;

		//Alloc data
		out->data[0] = img;
		out->data[1] = out->data[0] + numpixels;
		out->data[2] = out->data[1] + numpixels / 4;

		//Set size for planes
		out->linesize[0] = width;
		out->linesize[1] = width/2;
		out->linesize[2] = width/2;

		//Convert
		if (sws) sws_scale(sws, in->data, in->linesize, 0, height, out->data, out->linesize);

		//If we had canvas
		if (canvas)
		{
			//reset it
			canvas->Reset();
					
			//Get editor name
			std::wstring editor = server.GetEditorName();
			
			//If we got one
			if (!editor.empty())
			{
				DWORD x = lastX+16;
				DWORD y = lastY+16;
				DWORD w = 180;
				DWORD h = 36;
				DWORD m = 10;
				
				//Ensure the window is big enought
				if (width>w+m && height>h+m)
				{
					//Check overlay would be out of the image
					if (x+w+m>width)
						//Move to the left of the pointer
						x = x-w;
					//Check overlay would be out of the image
					if (y+h+m>height)
						//Move position
						y = y-h;	
					
					//Set text properties
					Properties properties;
					properties.SetProperty("fillColor"	,"#11FF1140");
					properties.SetProperty("strokeColor"	,"#40FF40A0");
					properties.SetProperty("color"		,"black");
					properties.SetProperty("font"		,"Verdana-Regular");
					properties.SetProperty("fontSize"	,18);
					
					//Draw editor name on canvas
					canvas->RenderText(editor,x,y,w,h,properties);

					//Draw it on top of the image
					canvas->Draw(img,img);
				}
				
				//Check coordinates
				if (lastX<width-1 && lastY<height-1)
				{
					//Draw mouse pointer
					out->data[0][lastY*out->linesize[0]+lastX] = 0;
					out->data[0][(lastY+1)*out->linesize[0]+lastX] = 0;
					out->data[0][lastY*out->linesize[0]+lastX+1] = 0;
					out->data[0][(lastY+1)*out->linesize[0]+lastX+1] = 0;
					out->data[1][lastY/2*out->linesize[1]+lastX/2] = 0;
					out->data[2][lastY/2*out->linesize[2]+lastX/2] = 0;
				}
			}
		}
		//Put new frame
		output->NextFrame(img);

		//Free frames
		av_frame_free(&in);
		av_frame_free(&out);
	}

	return 1;
}

//** Server -> Client */
int AppMixer::onHandleCursorPos(VNCViewer *viewer,int x, int y)
{
	//Do nothing
	return 1;
}

//** Client -> Server */
 void AppMixer::onMouseEvent(int buttonMask, int x, int y)
{
	//Lock
	use.WaitUnusedAndLock();

	//Send to server
	if (presenter)
		//Send ley
		presenter->SendMouseEvent(buttonMask,x,y);
	
	//Store latest coordinates
	lastX = x;
	lastY = y;
	
	//Unlock
	use.Unlock();
}

void AppMixer::onKeyboardEvent(bool down, DWORD keySym)
{
	//Lock
	use.WaitUnusedAndLock();

	//Send to server
	if (presenter)
		//Send ley
		presenter->SendKeyboardEvent(down,keySym);
	//Unlock
	use.Unlock();
}
