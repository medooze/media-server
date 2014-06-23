/* 
 * File:   VNCViewer.cpp
 * Author: Sergio
 * 
 * Created on 15 de noviembre de 2013, 12:37
 */

#include "VNCViewer.h"
#include "log.h"

extern "C" 
{
	static void Dummy(rfbClient* client) {
	}

	static rfbBool DummyPoint(rfbClient* client, int x, int y) {
		return TRUE;
	}

	static void DummyRect(rfbClient* client, int x, int y, int w, int h) {
	}

	static char* ReadPassword(rfbClient* client) {

	}

	int WaitForMessage(rfbClient* client,unsigned int usecs)
	{
		return !((VNCViewer::Socket*)client->tlsSession)->WaitForMessage(usecs);
	}
	
	rfbBool ReadFromRFBServer(rfbClient* client, char *out, unsigned int n)
	{
		return ((VNCViewer::Socket*)client->tlsSession)->ReadFromRFBServer(out,n);
	}
	rfbBool WriteToRFBServer(rfbClient* client, char *buf, int n)
	{
		return ((VNCViewer::Socket*)client->tlsSession)->WriteToRFBServer(buf,n);
	}
	int FindFreeTcpPort(void){ return 1; }
	int ListenAtTcpPort(int port){ return 1; }
	int ListenAtTcpPortAndAddress(int port, const char *address){ return 1; }
	int ConnectClientToTcpAddr(unsigned int host, int port){ return 1; }
	int ConnectClientToTcpAddr6(const char *hostname, int port){ return 1; }
	int ConnectClientToUnixSock(const char *sockFile){ return 1; }
	int AcceptTcpConnection(int listenSock){ return 1; }
	rfbBool SetNonBlocking(int sock){ return 1; }
	rfbBool SetDSCP(int sock, int dscp){ return 1; }
	rfbBool StringToIPAddr(const char *str, unsigned int *addr) { return 1; }
	rfbBool SameMachine(int sock)  { return 1; }
	rfbBool errorMessageOnReadFailure = TRUE;
	static void CopyRectangleFromRectangle(rfbClient* client, int src_x, int src_y, int w, int h, int dest_x, int dest_y) { }
}


VNCViewer::VNCViewer()
{
	//No client
	client = NULL;
	
	//Not running
	running = false;
}

rfbBool VNCViewer::MallocFrameBuffer(rfbClient* client)
{
	//Vet viewer
	VNCViewer * viewer = (VNCViewer *)(client->clientData);
	//Reset size
	return viewer->ResetSize();
}

void VNCViewer::FinishedFrameBufferUpdate(rfbClient* client)
{
	Debug("-FinishedFrameBufferUpdate\n");
	//Vet viewer
	VNCViewer * viewer = (VNCViewer *)(client->clientData);
	//Send event
	if (viewer->listener)
		//Call it
		viewer->listener->onFinishedFrameBufferUpdate(viewer);
}

void VNCViewer::GotFrameBufferUpdate(rfbClient* client, int x, int y, int w, int h)
{
	Debug("-GotFrameBufferUpdate [%d,%d,%d,%d]\n",x,y,w,h);
	//Vet viewer
	VNCViewer * viewer = (VNCViewer *)(client->clientData);
	//Send event
	if (viewer->listener)
		//Call it
		viewer->listener->onFrameBufferUpdate(viewer,x,y,w,h);
}


void VNCViewer::GotCopyRectangle(rfbClient* client, int src_x, int src_y, int w, int h, int dest_x, int dest_y)
{
	//Log
	Log("-GotCopyRectangle [x:%d,y;%d,w:%d,h:%d,to_x:%d,to_y:%d]\n",src_x, src_y, w, h, dest_x, dest_y);
	//Copy rectangle in framebuffer first
	CopyRectangleFromRectangle(client,src_x, src_y, w, h,dest_x, dest_y);
	//Vet viewer
	VNCViewer * viewer = (VNCViewer *)(client->clientData);
	//Send event
	if (viewer->listener)
		//Call it
		viewer->listener->onGotCopyRectangle(viewer,src_x, src_y, w, h, dest_x, dest_y);;
}

rfbBool VNCViewer::HandleCursorPos(rfbClient* client, int x, int y)
{
	//Vet viewer
	VNCViewer * viewer = (VNCViewer *)(client->clientData);
	//Send event
	if (viewer->listener)
		//Call it
		viewer->listener->onHandleCursorPos(viewer,x,y);

	//Ok
	return true;
}

int VNCViewer::Init(VNCViewer::Socket* socket, VNCViewer::Listener* listener)
{
	//Check if running
	if (running)
		//Fail
		return Error("-Already running\n");

	//LOg
	Log("-VNCViewer Init\n");

	//Hardcoded values
	int bitsPerSample = 8;
	int samplesPerPixel = 3;
        int bytesPerPixel = 4;
	
	//Create client
	client = (rfbClient*) calloc(sizeof (rfbClient), 1);

	client->appData.shareDesktop = TRUE;
        client->appData.viewOnly = FALSE;
        client->appData.encodingsString = "tight zrle ultra copyrect hextile zlib corre rre raw";
        client->appData.useBGR233 = FALSE;
        client->appData.nColours = 0;
        client->appData.forceOwnCmap = FALSE;
        client->appData.forceTrueColour = FALSE;
        client->appData.requestedDepth = 0;
        client->appData.compressLevel = 3;
        client->appData.qualityLevel = 5;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
        client->appData.enableJPEG = TRUE;
#else
        client->appData.enableJPEG = FALSE;
#endif
        client->appData.useRemoteCursor = FALSE;

        client->endianTest = 1;
        client->programName = "";
        client->serverHost = strdup("");
        client->serverPort = 5900;

        client->destHost = NULL;
        client->destPort = 5900;

        client->CurrentKeyboardLedState = 0;
        client->HandleKeyboardLedState = (HandleKeyboardLedStateProc) DummyPoint;

        /* default: use complete frame buffer */
        client->updateRect.x = -1;

        client->format.bitsPerPixel = bytesPerPixel * 8;
        client->format.depth = bitsPerSample*samplesPerPixel;
        client->appData.requestedDepth = client->format.depth;
        client->format.bigEndian = *(char *) &client->endianTest ? FALSE : TRUE;
        client->format.trueColour = TRUE;

        if (client->format.bitsPerPixel == 8) {
                client->format.redMax = 7;
                client->format.greenMax = 7;
                client->format.blueMax = 3;
                client->format.redShift = 0;
                client->format.greenShift = 3;
                client->format.blueShift = 6;
        } else {
                client->format.redMax = (1 << bitsPerSample) - 1;
                client->format.greenMax = (1 << bitsPerSample) - 1;
                client->format.blueMax = (1 << bitsPerSample) - 1;
                if (!client->format.bigEndian) {
                        client->format.redShift = 0;
                        client->format.greenShift = bitsPerSample;
                        client->format.blueShift = bitsPerSample * 2;
                } else {
                        if (client->format.bitsPerPixel == 8 * 3) {
                                client->format.redShift = bitsPerSample * 2;
                                client->format.greenShift = bitsPerSample * 1;
                                client->format.blueShift = 0;
                        } else {
                                client->format.redShift = bitsPerSample * 3;
                                client->format.greenShift = bitsPerSample * 2;
                                client->format.blueShift = bitsPerSample;
                        }
                }
        }

        client->bufoutptr = client->buf;
        client->buffered = 0;

#ifdef LIBVNCSERVER_HAVE_LIBZ
        client->raw_buffer_size = -1;
        client->decompStreamInited = FALSE;

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
        memset(client->zlibStreamActive, 0, sizeof (rfbBool)*4);
        client->jpegSrcManager = NULL;
#endif
#endif

	//Set up client
	client->canHandleNewFBSize = TRUE;
	client->FinishedFrameBufferUpdate = VNCViewer::FinishedFrameBufferUpdate;
	client->GotFrameBufferUpdate = VNCViewer::GotFrameBufferUpdate;
	client->GotCopyRect = VNCViewer::GotCopyRectangle;
	client->MallocFrameBuffer = VNCViewer::MallocFrameBuffer;

        client->HandleCursorPos = DummyPoint;
        client->SoftCursorLockArea = DummyRect;
        client->SoftCursorUnlockScreen = Dummy;
        client->GetPassword = ReadPassword;

        client->Bell = Dummy;
        client->CurrentKeyboardLedState = 0;
        client->HandleKeyboardLedState = (HandleKeyboardLedStateProc) DummyPoint;
        client->QoS_DSCP = 0;

        client->authScheme = 0;
        client->subAuthScheme = 0;
        client->GetCredential = NULL;
        client->tlsSession = NULL;
        client->sock = -1;
        client->listenSock = -1;
        client->listenAddress = NULL;
        client->listen6Sock = -1;
        client->listen6Address = NULL;
        client->clientAuthSchemes = NULL;

	//Store listener
	this->listener = listener;

	//Overwrite socket
	client->tlsSession = (void*)socket;
	
	//Set ups as client data ovverrriding its intended usage
	client->clientData = (rfbClientData *)this;

	//Already litening
	client->listenSpecified = true;

	//Start thread
	running = createPriorityThread(&thread,run,this,0);

        return running;
}

int VNCViewer::End()
{
	//If running
	if (!running)
		//Exit
		return 0;

	//Log
	Log(">End VNCViewer\n");
	
	//Stop
	running = 0;
	
	//Cancel any pending wait
	((Socket*)client->tlsSession)->CancelWaitForMessage();

	Log("-End VNCViewer joining thread\n");
	//Join run thread
	pthread_join(thread,NULL);
	Log("-End VNCViewer joined thread\n");

	#ifdef LIBVNCSERVER_HAVE_LIBZ
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
        int i;

        for (i = 0; i < 4; i++) {
                if (client->zlibStreamActive[i] == TRUE) {
                        if (inflateEnd(&client->zlibStream[i]) != Z_OK &&
                                client->zlibStream[i].msg != NULL)
                                rfbClientLog("inflateEnd: %s\n", client->zlibStream[i].msg);
                }
        }

        if (client->decompStreamInited == TRUE) {
                if (inflateEnd(&client->decompStream) != Z_OK &&
                        client->decompStream.msg != NULL)
                        rfbClientLog("inflateEnd: %s\n", client->decompStream.msg);
        }

        if (client->jpegSrcManager)
                free(client->jpegSrcManager);
#endif
#endif

	//Free other memory
	if (client->desktopName)	free(client->desktopName);
        if (client->serverHost)		free(client->serverHost);
        if (client->destHost)		free(client->destHost);
        if (client->clientAuthSchemes)	free(client->clientAuthSchemes);

	//Check if it already has allocated memory
	if (client->frameBuffer)
		//Clean it
		free(client->frameBuffer);

	//Free client
	free(client);

	//Log
	Log("<End VNCViewer\n");
}

VNCViewer::~VNCViewer()
{
	//End just in case
	End();
}

void *VNCViewer::run(void *par)
{
	Log("-VNCViewerThread [%p]\n",pthread_self());

	//Get object
	VNCViewer *viewer = (VNCViewer *)par;

	//Bloqueamos las se�ales
	blocksignals();

	//Ejecutamos
	viewer->Run();
	//Exit
	return NULL;
}

int VNCViewer::Run()
{
	Log(">VNCViewer run\n");

	//Init connection
	if (!InitialiseRFBConnection(client))
		//Error
                return Error("-VNCViewer error initializing\n");

	//Set initial size and alloc mem
        client->width = client->si.framebufferWidth;
        client->height = client->si.framebufferHeight;
        client->MallocFrameBuffer(client);

        if (!SetFormatAndEncodings(client))
		//Error
                return Error("-VNCViewer error initializing\n");

        if (client->updateRect.x < 0)
	{
                client->updateRect.x = client->updateRect.y = 0;
                client->updateRect.w = client->width;
                client->updateRect.h = client->height;
        }

        if (client->appData.scaleSetting > 1)
	{
                if (!SendScaleSetting(client, client->appData.scaleSetting))
			//Error
                        return Error("-VNCViewer error scaleSetting\n");
                if (!SendFramebufferUpdateRequest(client,
                        client->updateRect.x / client->appData.scaleSetting,
                        client->updateRect.y / client->appData.scaleSetting,
                        client->updateRect.w / client->appData.scaleSetting,
                        client->updateRect.h / client->appData.scaleSetting,
                        FALSE))
			//Error
                        return Error("-VNCViewer error SendFramebufferUpdateRequest scaleSetting\n");
        } else {
                if (!SendFramebufferUpdateRequest(client,
                        client->updateRect.x, client->updateRect.y,
                        client->updateRect.w, client->updateRect.h,
                        FALSE))
			//Error
                        return Error("-VNCViewer error SendFramebufferUpdateRequest\n");
        }

	//Check listener
	if (listener)
		//Started
		listener->onSharingStarted(this);

	//Loop 
	while(running)
	{
		//PRocessmessage
		if (!HandleRFBServerMessage(client))
		{
			//Error
			Error("HandleRFBServerMessage error\n");
			//Close socket
			((Socket*)client->tlsSession)->Close();
			//Exit
			break;
		}
	}

	//Check listener
	if (listener)
		//End it
		listener->onSharingEnded(this);

	//Log
	Log("<VNCViewer run");
	
	//OK
	return 1;
}

int VNCViewer::ResetSize()
{
	Log("-VNCViewer::ResetSize\n");
	
	//Check if it already has allocated memory
	if (client->frameBuffer)
		//Clean it
		free(client->frameBuffer);
	
	//Get num of pixels
	DWORD size = client->width*client->height*client->format.bitsPerPixel/8;

	//Allocate memory for RGB and YUV + padding
	client->frameBuffer = (BYTE*)malloc32(size+64);
	
	//Clean RGB
	memset(client->frameBuffer, 0, size);

	//Launc listener event
	if (listener)
		//Call it
		listener->onFrameBufferSizeChanged(this,client->width,client->height);
	
	//return result
	return true;
}

int VNCViewer::SendMouseEvent(int buttonMask, int x, int y)
{
	Log("-SendMouseEvent [x:%d,y:%d,mask:0x%x]\n",x,y,buttonMask);
	return SendPointerEvent(client,x,y,buttonMask);
}

int VNCViewer::SendKeyboardEvent(bool down, DWORD key)
{
	Log("-SendKeyboardEvent [key:%d,down:%d]\n",key,down);
	return SendKeyEvent(client,key,down);
}
