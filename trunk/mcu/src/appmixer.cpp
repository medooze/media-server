/*
 * File:   appmixer.cpp
 * Author: Sergio
 *
 * Created on 15 de enero de 2013, 15:38
 */

#include <netdb.h>

#include "appmixer.h"
#include "log.h"
#include "fifo.h"
#include "use.h"
#include "overlay.h"
#ifdef CEF
#include "cef/Browser.h"
#include "cef/keyboard.h"
#endif


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
	lastMask = 0;
	//No img
	img = NULL;
	//Clean popup
	popUpShown = false;
	popUpX = 0;
	popUpY = 0;
	popUpWidth = 0;
	popUpHeight = 0;
	
	//No modifiers for key/mouse
	modifiers = 0;
}
AppMixer::~AppMixer()
{
	//End just in case
	End();
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
	
	//Check max size
	if (logo.GetWidth()*logo.GetHeight()>4096*3072)
	{
		//CLose
		logo.Close();
		//Error
		return Error("-Size bigger than max size allowed (4096*3072)\n");
	}

	//Set size
	output->SetVideoSize(logo.GetWidth(),logo.GetHeight());
	//Set image
	output->NextFrame(logo.GetFrame());

	//Set size 
	SetSize(logo.GetWidth(),logo.GetHeight());
	//Set new frame
	server.FrameBufferUpdate(logo.GetFrameRGBA(),0,0,logo.GetWidth(),0,0,logo.GetWidth(),logo.GetHeight());
	//End update
	server.FrameBufferUpdateDone();
	
	//CLose
	logo.Close();

	//Everything ok
	return true;
}

int AppMixer::End()
{
	Log(">AppMixer::End\n");

	//Reset 
	Reset();
	
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

#ifdef CEF
	//If still showing the browser
	if (browser.get())
	{
		//Force Close
		browser->GetHost()->CloseBrowser(true);
		//Stop listening events
		browser = NULL;
	}
#endif
	//Unlock
	use.Unlock();

	//End VNC server
	server.End();
	
	Log("<AppMixer::End\n");

}

int AppMixer::Reset()
{
	Log("-AppMixer::Reset\n");
	
	//If already got memory
	if (img)
	{
		//Free it
		free(img);
		//Null it
		img = NULL;
	}
	//If got canvas
	if (canvas)
	{
		//Delete it
		delete(canvas);
		//null it
		canvas = NULL;
	}
}

int AppMixer::WebsocketConnectRequest(int partId,const std::wstring &name,WebSocket *ws,bool isPresenter,const std::string &to)
{
	Log("-WebsocketConnectRequest [partId:%d,isPresenter:%d]\n",partId,isPresenter);

	//Check if it is a viewer
	if (!isPresenter)
		//Let the server handle it
		return server.Connect(partId,name,ws,to);

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

int AppMixer::SetViewer(int viewerId)
{
	Log("-AppMixer::SetViewer [%d]\n",viewerId);
	
	return server.SetViewer(viewerId);
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
	return 1;
}

int AppMixer::onSharingEnded(VNCViewer *viewer)
{
	//Log
	Log("-onSharingEnded\n");
	//End sharing
	server.Reset();
	//Reset
	Reset();
	//Emulate frame buffer update
	if (output)
		//Clear frame
		output->ClearFrame();
	return 1;
}

int AppMixer::onFrameBufferSizeChanged(VNCViewer *viewer, int width, int height)
{
	Log("-onFrameBufferSizeChanged [%d,%d]\n",width,height);
	//Setting size
	return SetSize(width,height);
}


int AppMixer::SetSize(int width,int height)
{
	Log("-AppMixer::SetSize [%dx%d]\n",width,height);
	//Change server size
	server.SetSize(width,height);

	//REset us
	Reset();

	//If got size
	if (width && height)
	{
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
		
		//And create a new one
		canvas = new Canvas(width,height);
	}


	return 1;
}

int AppMixer::onFrameBufferUpdate(VNCViewer *viewer,int x, int y, int w, int h)
{
	//Log("-onFrameBufferUpdate [%d,%d,%d,%d]\n",x,y,w,h);

	//UPdate vnc server frame buffer
	server.FrameBufferUpdate(viewer->GetFrameBuffer(),x,y,server.GetWidth(),x,y,w,h);

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
	//Display image
	return Display(viewer->GetFrameBuffer(),0,0,viewer->GetWidth(),viewer->GetHeight(),viewer->GetWidth());
}


int AppMixer::Display(const BYTE* frame,int x, int y, int width,int height, int lineSize)
{
	if (output)
	{
		Debug("-Display [x:%d,y:%d,width:%d,height:%d,lineSize:%d]\n",x,y,width, height, lineSize);
		//Get server size
		DWORD w = server.GetWidth();
		DWORD h = server.GetHeight();
		//Calc num pixels
		DWORD numpixels = w*h;

		//Alloc data
		AVFrame* in = av_frame_alloc();
		AVFrame* out = av_frame_alloc();

		//Set in frame
		in->data[0] = (BYTE*)frame;

		//Set size
		in->linesize[0] = lineSize*4;

		//Alloc data
		out->data[0] = img + x + y*w;
		out->data[1] = img + numpixels + x/2 + y*w/4;
		out->data[2] = img + numpixels*5/4  + x/2 + y*w/4;

		//Set size for planes
		out->linesize[0] = w;
		out->linesize[1] = w/2;
		out->linesize[2] = w/2;

		// Create YUV rescaller cotext
		SwsContext* sws = sws_alloc_context();

		// Set property's of YUV rescaller context
		av_opt_set_defaults(sws);
		av_opt_set_int(sws, "srcw",       width			,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "srch",       height		,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "src_format", AV_PIX_FMT_RGBA	,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "dstw",       width			,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "dsth",       height		,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "dst_format", AV_PIX_FMT_YUV420P	,AV_OPT_SEARCH_CHILDREN);
		av_opt_set_int(sws, "sws_flags",  SWS_FAST_BILINEAR	,AV_OPT_SEARCH_CHILDREN);
	
		// Init YUV rescaller context
		if (sws_init_context(sws, NULL, NULL) < 0)
		{
			//Clear context
			sws_freeContext(sws);
			//Set errror
			return  Error("Couldn't init sws context\n");
		}

		//Convert
		sws_scale(sws, in->data, in->linesize, 0, height, out->data, out->linesize);

		//Clear context
		sws_freeContext(sws);

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
				DWORD w = 180;
				DWORD h = 36;
				DWORD o = 16;		//Offset
				DWORD m = 10;		//Margin
				
				//Ensure the window is big enought
				if (width>w+m && height>h+m)
				{
					//Add offset
					int x = lastX+o;
					int y = lastY+o;
					//Check overlay would be out of the image
					if (x+w+m>=width)
						//Move to the left of the pointer or width
						x = fmin(lastX,width)-o-w;
					//If we are not big enought
					if (x<0)
						//Reset to margin
						x = m-1;
					//Check overlay would be out of the image
					if (y+h+m>=height)
						//Move position to up the last pointer of height
						y = fmin(lastY,height)-o-h;
					//If we are not big enought
					if (y<0)
						//Reset to margin
						y = m-1;
					
					//Set text properties
					Properties properties;
					properties.SetProperty("fillColor"	,"#11FF1140");
					properties.SetProperty("strokeColor"	,"#40FF40A0");
					properties.SetProperty("color"		,"black");
					properties.SetProperty("font"		,"Verdana");
					properties.SetProperty("fontSize"	,18);
					
					Debug("-RenderText [x:%d,y:%d,w:%d,h:%d,width:%d,height:%d,lastX:%d,lastY:%d]\n",x,y,w,h,width,height,lastX,lastY);
					
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

	Debug("-AppMixer::onMouseEvent [x:%d,y:%d,mask:%d]\n",x,y,buttonMask);
	/*
	* Indicates either pointer movement or a pointer button press or release. The pointer is
	* now at (x-position, y-position), and the current state of buttons 1 to 8 are represented
	* by bits 0 to 7 of button-mask respectively, 0 meaning up, 1 meaning down (pressed).
	* On a conventional mouse, buttons 1, 2 and 3 correspond to the left, middle and right
	* buttons on the mouse. On a wheel mouse, each step of the wheel upwards is represented
	* by a press and release of button 4, and each step downwards is represented by
	* a press and release of button 5.
	*/

	//Lock
	use.WaitUnusedAndLock();

	//Send to server
	if (presenter)
		//Send ley
		presenter->SendMouseEvent(buttonMask,x,y);
#ifdef CEF
	//If doing in-server web browsing
	if (browser.get())
	{
		Debug("-AppMixer::onMouseEvent  [last:%d,mask:%d]\n",lastMask,buttonMask);
		CefMouseEvent event;
		//Set coordinates
		event.x = x;
		event.y = y;
		//Set modifiers

		//Check if any button has changed
		if (lastMask!=buttonMask) 
		{
			//For each button
			for (int i=1;i<6;i++) 
			{
				//Get old and new states
				BYTE oldState = lastMask   & (1<<(i-1));
				BYTE newState = buttonMask & (1<<(i-1)); 

				Debug("-CEF Mouse button [%d,%d] up [from:%d,to:%d]\n",i,(1<<(i-1)),oldState,newState);
				//If it has changed
				if (oldState!=newState)
				{
					//It is a click
					CefBrowserHost::MouseButtonType button;
					//Check which button
					switch (i)
					{
						case 1:
							//Check if now we are clicked
							if (newState)
								//Change modifiers
								modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
							else
								//Unset modifiers
								modifiers &= ~EVENTFLAG_LEFT_MOUSE_BUTTON;
							//Set event modifiers
							event.modifiers = modifiers;
							//Left button clicked 
							browser->GetHost()->SendMouseClickEvent(event,MBT_LEFT,!newState,1);
							break;
						case 2:	
							//Check if now we are clicked
							if (newState)
								//Change modifiers
								modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
							else
								//Unset modifiers
								modifiers &= ~EVENTFLAG_MIDDLE_MOUSE_BUTTON;
							//Set event modifiers
							event.modifiers = modifiers;
							//Middle button
							browser->GetHost()->SendMouseClickEvent(event,MBT_MIDDLE,!newState,1);
							break;
						case 3:
							//Check if now we are clicked
							if (newState)
								//Change modifiers
								modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
							else
								//Unset modifiers
								modifiers &= ~EVENTFLAG_RIGHT_MOUSE_BUTTON;
							//Set event modifiers
							event.modifiers = modifiers;
							//Right button
							browser->GetHost()->SendMouseClickEvent(event,MBT_RIGHT,!newState,1);
							break;
						case 4: 
							//Set event modifiers
							event.modifiers = modifiers;
							//Only on down event
							if (newState)
								//Mouse wheel up
								browser->GetHost()->SendMouseWheelEvent(event,0,40);
							break;
						case 5:
							//Set event modifiers
							event.modifiers = modifiers;
							//Only on down event
							if (newState)
								//MOuse wheel down
								browser->GetHost()->SendMouseWheelEvent(event,0,-40);
							break;
					}
				}
			}
		} else {
			Debug("-CEF Mouse move [x:%d,y:%d]\n",x,y);
			//Set event modifiers
			event.modifiers = modifiers;
			//Send only a mouse move
			browser->GetHost()->SendMouseMoveEvent(event,false);
		}
		
	}
#endif
	
	//Store latest coordinates
	lastX = x;
	lastY = y;
	lastMask = buttonMask;
	
	//Unlock
	use.Unlock();
}

void AppMixer::onKeyboardEvent(bool down, DWORD keySym)
{
	Debug("-AppMixer::onKeyboardEvent [down:%d,keySym:%d]\n",down,keySym);
	//Lock
	use.WaitUnusedAndLock();

	//Send to server
	if (presenter)
		//Send key
		presenter->SendKeyboardEvent(down,keySym);

#ifdef CEF
	//If doing in-server web browsing
	if (browser.get())
	{
  		CefKeyEvent key_event;
		//Get host
		CefRefPtr<CefBrowserHost> host = browser->GetHost();

		//Get keycode from keysym
  		KeyboardCode windows_key_code = KeyboardCodeFromXKeysym(keySym);
		//Set event values
  		key_event.windows_key_code = GetWindowsKeyCodeWithoutLocation(windows_key_code);
  		key_event.native_key_code = windows_key_code;
		key_event.character = keySym;
		//Always false on no-Windows
    		key_event.is_system_key = false;
		// We need to treat the enter key as a key press of character \r.  This
		// is apparently just how webkit handles it and what it expects.
		if (windows_key_code == VKEY_RETURN) 
			key_event.unmodified_character = '\r';
		else
			key_event.unmodified_character = keySym;

		//Calculate modifiers
		DWORD mod = 0;
		switch(windows_key_code)
		{
			case VKEY_CAPITAL:
				Debug("-AppMixer::onKeyboardEvent VKEY_CAPITAL mod EVENTFLAG_CAPS_LOCK_ON\n");
				mod = EVENTFLAG_CAPS_LOCK_ON;
				break;
			case VKEY_SHIFT:
				Debug("-AppMixer::onKeyboardEvent VKEY_SHIFT mod EVENTFLAG_SHIFT_DOWN\n");
				mod = EVENTFLAG_SHIFT_DOWN;
				break;
			case VKEY_CONTROL:
				Debug("-AppMixer::onKeyboardEvent VKEY_CONTROL mod EVENTFLAG_CONTROL_DOWN\n");
				mod = EVENTFLAG_CONTROL_DOWN;
				break;
			case VKEY_MENU:
				Debug("-AppMixer::onKeyboardEvent VKEY_MENU mod EVENTFLAG_ALT_DOWN\n");
				mod = EVENTFLAG_ALT_DOWN;
				break; 
			case VKEY_COMMAND:
				Debug("-AppMixer::onKeyboardEvent VKEY_COMMAND mod EVENTFLAG_COMMAND_DOWN\n");
				mod = EVENTFLAG_COMMAND_DOWN;
				break;
			case VKEY_NUMLOCK:
				Debug("-AppMixer::onKeyboardEvent VKEY_NUMLOCK mod EVENTFLAG_NUM_LOCK_ON\n");
				mod = EVENTFLAG_NUM_LOCK_ON;
				break;
		}

		//Check if it is a keypad what we are pressing
		if (IsXKeysymKeyPad(keySym))
		{
			Debug("-AppMixer::onKeyboardEvent mod EVENTFLAG_IS_KEY_PAD\n");
			//It is a keypad
			mod = EVENTFLAG_IS_KEY_PAD;
		}

		// If ctrl key is pressed down, then control character shall be input.
		if (key_event.modifiers & EVENTFLAG_CONTROL_DOWN)
			key_event.character = GetControlCharacter(windows_key_code, key_event.modifiers & EVENTFLAG_SHIFT_DOWN);

		//Modify modifiers
		if (down)
			//Add it
			modifiers |= mod;
		else
			//Remove it
			modifiers &= ~mod;
		

		//Set event modifiers
		key_event.modifiers = modifiers;

		Debug("-AppMixer::onKeyboardEvent sending key: [code:%d,keySym:%d,modifiers:%d,system:%d]\n", key_event.windows_key_code,keySym,key_event.modifiers,key_event.is_system_key);
		//Check if it is keydown or up	
		if (down) {
			//Send down 
			key_event.type = KEYEVENT_RAWKEYDOWN;
			host->SendKeyEvent(key_event);
			//Check if it is char key
			switch(key_event.windows_key_code)
			{
				case VKEY_BACK :
				case VKEY_TAB :
				case VKEY_BACKTAB :
				case VKEY_CLEAR :
				case VKEY_RETURN :
				case VKEY_SHIFT :
				case VKEY_CONTROL :
				case VKEY_MENU :
				case VKEY_PAUSE :
				case VKEY_CAPITAL :
				case VKEY_KANA :
				case VKEY_FINAL :
				case VKEY_HANJA :
				case VKEY_ESCAPE :
				case VKEY_CONVERT :
				case VKEY_NONCONVERT :
				case VKEY_ACCEPT :
				case VKEY_MODECHANGE :
				case VKEY_PRIOR :
				case VKEY_NEXT :
				case VKEY_END :
				case VKEY_HOME :
				case VKEY_LEFT :
				case VKEY_UP :
				case VKEY_RIGHT :
				case VKEY_DOWN :
				case VKEY_SELECT :
				case VKEY_PRINT :
				case VKEY_EXECUTE :
				case VKEY_SNAPSHOT :
				case VKEY_INSERT :
				case VKEY_DELETE :
				case VKEY_HELP :
				case VKEY_LWIN :
				case VKEY_RWIN :
				case VKEY_APPS :
				case VKEY_SLEEP :
				case VKEY_NUMPAD0 :
				case VKEY_NUMPAD1 :
				case VKEY_NUMPAD2 :
				case VKEY_NUMPAD3 :
				case VKEY_NUMPAD4 :
				case VKEY_NUMPAD5 :
				case VKEY_NUMPAD6 :
				case VKEY_NUMPAD7 :
				case VKEY_NUMPAD8 :
				case VKEY_NUMPAD9 :
				case VKEY_F1 :
				case VKEY_F2 :
				case VKEY_F3 :
				case VKEY_F4 :
				case VKEY_F5 :
				case VKEY_F6 :
				case VKEY_F7 :
				case VKEY_F8 :
				case VKEY_F9 :
				case VKEY_F10 :
				case VKEY_F11 :
				case VKEY_F12 :
				case VKEY_F13 :
				case VKEY_F14 :
				case VKEY_F15 :
				case VKEY_F16 :
				case VKEY_F17 :
				case VKEY_F18 :
				case VKEY_F19 :
				case VKEY_F20 :
				case VKEY_F21 :
				case VKEY_F22 :
				case VKEY_F23 :
				case VKEY_F24 :
				case VKEY_NUMLOCK :
				case VKEY_SCROLL :
				case VKEY_LSHIFT :
				case VKEY_RSHIFT :
				case VKEY_LCONTROL :
				case VKEY_RCONTROL :
				case VKEY_LMENU :
				case VKEY_RMENU :
				case VKEY_BROWSER_BACK :
				case VKEY_BROWSER_FORWARD :
				case VKEY_BROWSER_REFRESH :
				case VKEY_BROWSER_STOP :
				case VKEY_BROWSER_SEARCH :
				case VKEY_BROWSER_FAVORITES :
				case VKEY_BROWSER_HOME :
				case VKEY_VOLUME_MUTE :
				case VKEY_VOLUME_DOWN :
				case VKEY_VOLUME_UP :
				case VKEY_MEDIA_NEXT_TRACK :
				case VKEY_MEDIA_PREV_TRACK :
				case VKEY_MEDIA_STOP :
				case VKEY_MEDIA_PLAY_PAUSE :
				case VKEY_MEDIA_LAUNCH_MAIL :
				case VKEY_MEDIA_LAUNCH_MEDIA_SELECT :
				case VKEY_MEDIA_LAUNCH_APP1 :
				case VKEY_MEDIA_LAUNCH_APP2 :
				case VKEY_PROCESSKEY :
				case VKEY_PACKET :
				case VKEY_DBE_SBCSCHAR :
				case VKEY_DBE_DBCSCHAR :
				case VKEY_ATTN :
				case VKEY_CRSEL :
				case VKEY_EXSEL :
				case VKEY_EREOF :
				case VKEY_PLAY :
				case VKEY_ZOOM :
				case VKEY_NONAME :
				case VKEY_PA1 :
				case VKEY_OEM_CLEAR :
				// POSIX specificcase VKEYs. Note that as of Windows SDK 7.1, 0x97-9F, 0xD8-DA,
				// and 0xE8 are unassigned.
				case VKEY_WLAN :
				case VKEY_POWER :
				case VKEY_BRIGHTNESS_DOWN :
				case VKEY_BRIGHTNESS_UP :
				case VKEY_KBD_BRIGHTNESS_DOWN :
				case VKEY_KBD_BRIGHTNESS_UP :
				// Windows does not have a specific key code for AltGr. We use the unused 0xE1
				// (VK_OEM_AX) code to represent AltGr, matching the behaviour of Firefox on
				// Linux.
				case VKEY_ALTGR :
				// Windows does not have a specific key code for Compose. We use the unused
				// 0xE6 (VK_ICO_CLEAR) code to represent Compose.
				case VKEY_COMPOSE :
					//Skip
					Debug("-Skiping char\n");
					break;
				default:
					//Send char
					key_event.type = KEYEVENT_CHAR;
					host->SendKeyEvent(key_event);
			}
		} else {
			//Send only up
			key_event.type = KEYEVENT_KEYUP;
			host->SendKeyEvent(key_event);
		}
	}
#endif

	//Unlock
	use.Unlock();
}


int AppMixer::OpenURL(const char* url)
{
#ifdef CEF
	//Lock
	use.WaitUnusedAndLock();

	//Set size
	SetSize(1024,768);

	//If we don't have a browser instance yet
	if (!browser.get())
	{
		//Get browser instance
		Browser& instance = Browser::getInstance();
		
		//Create the new window
		instance.CreateFrame(url,server.GetWidth(), server.GetHeight(),this);
	} else {
		//Navigate to that URL
		browser->GetMainFrame()->LoadURL(url);
	}
	//Clean popup
	popUpShown = false;
	popUpX = 0;
	popUpY = 0;
	popUpWidth = 0;
	popUpHeight = 0;

	//Unlock
	use.Unlock();
	//Ok
	return true;
#else
	//Error	
	return Error("CEF nor supported\n");
#endif
}

int AppMixer::CloseURL()
{
#ifdef CEF
	//Reset size
	server.Reset();

	//Lock
	use.WaitUnusedAndLock();

	//Ensure we have a browser
	if (browser.get())
		//Force Close
		browser->GetHost()->CloseBrowser(true);
	//Unlock
	use.Unlock();

	//Clean popup
	popUpShown = false;
	popUpX = 0;
	popUpY = 0;
	popUpWidth = 0;
	popUpHeight = 0;

	//OK
	return true;
#else
	//Error
	return Error("CEF nor supported\n");
#endif
}

#ifdef CEF
bool AppMixer::GetViewRect(CefRect& rect)
{
	Log("-AppMixer::GetViewRect [%d,%d]\n",server.GetWidth(),server.GetHeight());
	//Set server dimensions
	rect = CefRect(0, 0, server.GetWidth(), server.GetHeight());
	//Ensure we have a browser
	if (browser.get())
	{
		//Send focus event
		browser->GetHost()->SetFocus(true);
		browser->GetHost()->SendFocusEvent(true);
	}
	//Changed
	return true;
}

void AppMixer::OnPaint(CefRenderHandler::PaintElementType type, const CefRenderHandler::RectList& rects, const BYTE* buffer, int width, int height)
{
	DWORD x = 0;
	DWORD y = 0;

	Debug("-AppMixer::OnPaint [view:%d,popup:%d,width:%d,height:%d,popUpX:%d,popUpY:%d]\n",type==PET_VIEW,type==PET_POPUP,width,height,popUpX,popUpY);
	
	//If it is the pop uyp
	if (type == PET_POPUP)
	{
		//Set pop up offset
		x = popUpX;
		y = popUpY;
	}

	//BGRA to RGBA
	for (int j=0;j<height;++j)
	{
		//Get start of line
		BYTE *line = (BYTE*)buffer+j*width*4;
		//Each pixel
		for (int i=0;i<width;++i)
		{	
			//Get red and blue
			BYTE *R = line+i*4;
			BYTE *B = R+2;
			//Swap
			BYTE aux = *R;
			*R = *B;
			*B = aux;
		}
	}
	
	// Update just the dirty rectangles.
	for (CefRenderHandler::RectList::const_iterator i = rects.begin() ; i != rects.end(); ++i) 
	{
		//Get rectangle
		const CefRect& rect = *i;
		//UPdate vnc server frame buffer
		server.FrameBufferUpdate(buffer, rect.x, rect.y, width, rect.x+x, rect.y+y, rect.width, rect.height);

		//If the main window has been updated and covered the pop up
		if (popUpShown && type==PET_VIEW && 
			//OVerlaps in X
			( 
				(rect.x<popUpX  && rect.x+rect.width>=popUpX) || 
				(rect.x>=popUpX && rect.x<=popUpX+popUpWidth)
			) && 
			//Overlaps in Y
			( 
				(rect.y<popUpY  && rect.y+rect.height>=popUpY) || 
				(rect.y>=popUpY && rect.y<=popUpY+popUpHeight)
			) 
		)
			//We are drawing under the pop up, redraw it
			browser->GetHost()->Invalidate(PET_POPUP);
	}
	
	//Signal server
	server.FrameBufferUpdateDone();

	//And display image
	Display(buffer,x,y,width,height,width);

}

void AppMixer::OnBrowserCreated(CefRefPtr<CefBrowser> browser)
{  
	Log("-AppMixer::OnBrowserCreated [browser:%p,id:%d]\n",browser.get(),browser.get()->GetIdentifier());

	//Lock
	use.WaitUnusedAndLock();
	
	//Only if we don't have a browser yet
	if (!this->browser.get())	
	{
		//Set it
		this->browser = browser;

		//Send focus event
		browser->GetHost()->SetFocus(true);
		browser->GetHost()->SendFocusEvent(true);
	} else {
		//Force Close
		browser->GetHost()->CloseBrowser(true);
	}

	//Unlock
	use.Unlock();
	
}

void AppMixer::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser)
{
	Log("-AppMixer::OnBrowserDestroyed [browser:%p,id:%d]\n",browser.get(),browser.get()->GetIdentifier());
	//Lock
	use.WaitUnusedAndLock();

	//If it was ours
	if (this->browser.get() && this->browser.get()->GetIdentifier()==browser.get()->GetIdentifier())
		//Release ref
		this->browser = NULL;

	//Unlock
	use.Unlock();
}
void AppMixer::OnPopupShow(bool show)
{
	 Log("-AppMixer::OnPopupShow [%d]\n",show);
	//Store it
	popUpShown = show;
	//If hiden it
	if (!popUpShown)
		//Refresh the whole view
		browser->GetHost()->Invalidate(PET_VIEW);
}

void AppMixer::OnPopupSize(const CefRect& rect)
{
	 Log("-AppMixer::OnPopupSize [%d,%d,%d,%d]\n",rect.x,rect.y,rect.width,rect.height);
	//Store popup position
	popUpX = rect.x;
	popUpY = rect.y;
	popUpWidth = rect.width;
	popUpHeight = rect.height;
}
#endif
