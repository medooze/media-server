/* 
 * File:   VNCServer.cpp
 * Author: Sergio
 * 
 * Created on 4 de diciembre de 2013, 15:51
 */
#include "log.h"
#include "errno.h"
#include "VNCServer.h"


extern "C"
{
	#include "rfb/rfbregion.h"
	/* sockets.c */
	 int rfbMaxClientWait = 100;
	 void rfbInitSockets(rfbScreenInfoPtr rfbScreen){}
	 void rfbShutdownSockets(rfbScreenInfoPtr rfbScreen){}
	 void rfbDisconnectUDPSock(rfbScreenInfoPtr rfbScreen){}

	 void rfbCloseClient(rfbClientPtr cl)
	 {
		 ((VNCServer::Client*)(cl->clientData))->Close();
	 }

	 int rfbReadExact(rfbClientPtr cl, char *buf, int len)
	 {
		if(cl->screen && cl->screen->maxClientWait)
			return(rfbReadExactTimeout(cl,buf,len,cl->screen->maxClientWait));
		 else
			return(rfbReadExactTimeout(cl,buf,len,0));
	 }

	 int rfbReadExactTimeout(rfbClientPtr cl, char *buf, int len,int timeout)
	 {
		 return ((VNCServer::Client*)(cl->clientData))->Read(buf,len,timeout);
	 }

	 int rfbPeekExactTimeout(rfbClientPtr cl, char *buf, int len,int timeout){return 1;}

	 int rfbWriteExact(rfbClientPtr cl, const char *buf, int len)
	 {
		return ((VNCServer::Client*)(cl->clientData))->Write(buf,len) ? 0 : -1;
	 }

	 int rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec){return 1;}
	 int rfbConnect(rfbScreenInfoPtr rfbScreen, char* host, int port){return 1;}
	 int rfbConnectToTcpAddr(char* host, int port){return 1;}
	 int rfbListenOnTCPPort(int port, in_addr_t iface){return 1;}
	 int rfbListenOnTCP6Port(int port, const char* iface){return 1;}
	 int rfbListenOnUDPPort(int port, in_addr_t iface){return 1;}
	 int rfbStringToAddr(char* string,in_addr_t* addr){return 1;}
	 rfbBool rfbSetNonBlocking(int sock){return 1;}
	 /* httpd.c */
	 void rfbHttpInitSockets(rfbScreenInfoPtr rfbScreen){}
	 void rfbHttpShutdownSockets(rfbScreenInfoPtr rfbScreen){}
	 void rfbHttpCheckFds(rfbScreenInfoPtr rfbScreen){}

	 /* response is cl->authChallenge vncEncrypted with passwd */
	extern rfbBool rfbDefaultPasswordCheck(rfbClientPtr cl,const char* response,int len);
	extern void rfbInitServerFormat(rfbScreenInfoPtr screen, int bitsPerSample);
	extern void rfbDefaultKbdAddEvent(rfbBool down, rfbKeySym keySym, rfbClientPtr cl);
	extern void rfbDefaultSetXCutText(char* text, int len, rfbClientPtr cl);
	extern rfbCursorPtr rfbDefaultGetCursorPtr(rfbClientPtr cl);
}

static rfbCursor myCursor =
{
   FALSE, FALSE, FALSE, FALSE,
   (unsigned char*)"\000\102\044\030\044\102\000",
   (unsigned char*)"\347\347\176\074\176\347\347",
   8, 7, 3, 3,
   0, 0, 0,
   0xffff, 0xffff, 0xffff,
   NULL
};

VNCServer::VNCServer()
{
	int width=400;
	int height=200;
	int bytesPerPixel = 4;
	int bitsPerSample  = 8;

	//Alocate screen info
	screen = (rfbScreenInfo*)calloc(sizeof(rfbScreenInfo),1);

	//Set private data to use object to allow locking in static objects
	screen->screenData = (void*)&use;

	//Set default vaules
	screen->autoPort=FALSE;
	screen->clientHead=NULL;
	screen->pointerClient=NULL;
	screen->port=5900;
	screen->ipv6port=5900;
	screen->socketState=RFB_SOCKET_INIT;

	screen->inetdInitDone = FALSE;
	screen->inetdSock=-1;

	screen->udpSock=-1;
	screen->udpSockConnected=FALSE;
	screen->udpPort=0;
	screen->udpClient=NULL;

	screen->maxFd=0;
	screen->listenSock=-1;
	screen->listen6Sock=-1;

	screen->httpInitDone=FALSE;
	screen->httpEnableProxyConnect=FALSE;
	screen->httpPort=0;
	screen->http6Port=0;
	screen->httpDir=NULL;
	screen->httpListenSock=-1;
	screen->httpListen6Sock=-1;
	screen->httpSock=-1;

	screen->desktopName = "MCU";
	screen->alwaysShared = FALSE;
	screen->neverShared = FALSE;
	screen->dontDisconnect = FALSE;
	screen->authPasswdData = NULL;
	screen->authPasswdFirstViewOnly = 1;

	screen->width = width;
	screen->height = height;
	screen->bitsPerPixel = screen->depth = 8*bytesPerPixel;

	//Allocate memory
	screen->frameBuffer = (char*) malloc(screen->width*screen->height*screen->depth/8);

	screen->passwordCheck = rfbDefaultPasswordCheck;

	screen->ignoreSIGPIPE = TRUE;

	/* disable progressive updating per default */
	screen->progressiveSliceHeight = 0;

	screen->listenInterface = htonl(INADDR_ANY);

	screen->deferUpdateTime=0;
	screen->maxRectsPerUpdate=50;

	screen->handleEventsEagerly = FALSE;

	screen->protocolMajorVersion = rfbProtocolMajorVersion;
	screen->protocolMinorVersion = rfbProtocolMinorVersion;

	screen->permitFileTransfer = FALSE;

	screen->paddedWidthInBytes = width*bytesPerPixel;

	/* format */

	rfbInitServerFormat(screen, bitsPerSample);

	/* cursor */

	screen->cursorX=screen->cursorY=screen->underCursorBufferLen=0;
	screen->underCursorBuffer=NULL;
	screen->dontConvertRichCursorToXCursor = FALSE;
	screen->cursor = &myCursor;
	INIT_MUTEX(screen->cursorMutex);

	IF_PTHREADS(screen->backgroundLoop = FALSE);

	/* proc's and hook's */

	screen->kbdAddEvent = rfbDefaultKbdAddEvent;
	screen->kbdReleaseAllKeys = rfbDoNothingWithClient;
	screen->ptrAddEvent = onMouseEvent;
	screen->setXCutText = rfbDefaultSetXCutText;
	screen->getCursorPtr = rfbDefaultGetCursorPtr;
	screen->setTranslateFunction = rfbSetTranslateFunction;
	screen->newClientHook = NULL;
	screen->displayHook = NULL;
	screen->displayFinishedHook = NULL;
	screen->getKeyboardLedStateHook = NULL;
	screen->xvpHook = NULL;

	/* initialize client list and iterator mutex */
	rfbClientListInit(screen);
}

VNCServer::~VNCServer()
{
}
int VNCServer::Init()
{
	
}

int VNCServer::Connect(int partId,WebSocket *socket)
{
	Log("-VNCServer connecting participant viewer [id:%d]\n",partId);
	//Lock
	use.IncUse();
	//Create client
	Client *client = new Client(partId,screen);
	//Set client as as user data
	socket->SetUserData(client);
	//Append to client list
	clients[partId] = client;
	//Accept incoming connection and add us as listeners
	socket->Accept(this);
	//Unlock
	use.DecUse();
	//Connect to socket
	return client->Connect(socket);
}

int VNCServer::Disconnect(WebSocket *socket)
{
	//Get client
	Client *client = (Client *)socket->GetUserData();
	//Disconnect
	client->Close();
	//Lock
	use.WaitUnusedAndLock();
	//Remove it
	Clients::iterator it = clients.find(client->GetId());
	//Check if found
	if (it!=clients.end())
	{
		//Get client
		Client *client = it->second;
		//Erase it
		clients.erase(it);
		//Delete it
		delete(client);
	}
	//Unlock
	use.Unlock();
	//OK
	return 1;
}
int VNCServer::SetSize(int width,int height)
{
	Log("-VNCServer::SetSize [%d,%d]\n",width,height);
	
	//Lock
	use.WaitUnusedAndLock();

	//Create new screen
	rfbScreenInfo* screen2 = (rfbScreenInfo*)malloc(sizeof(rfbScreenInfo));

	//Copy contents of first screen
	memcpy(screen2,screen,sizeof(rfbScreenInfo));

	//Set new size
	screen->width = width;
	screen->height = height;

	//Allocate new framebuffer
	screen2->frameBuffer = (char*) malloc(screen->width*screen->height*screen->depth/8);

	//Check if it is been used by any client
	if (screen->scaledScreenRefCount)
	{
		//Free framebuffer
		if (screen->frameBuffer)
			free(screen->frameBuffer);
		//Delete screen
		free(screen);
	}

	//Set new screen
	screen = screen2;

	//Resize all viewers
	for (Clients::iterator it=clients.begin(); it!=clients.end(); ++it)
		//Update it
		it->second->ResizeScreen(screen);

	//Unlock
	use.Unlock();

	//OK
	return 1;
}

int VNCServer::FrameBufferUpdateDone()
{
	//Send and update to all viewers
	for (Clients::iterator it=clients.begin(); it!=clients.end(); ++it)
		//Update it
		it->second->Update();
}

int VNCServer::FrameBufferUpdate(BYTE *data,int x,int y,int width,int height)
{
	//LOck
	use.IncUse();

	//Update frame
	for (int j=y;j<y+height;++j)
		//Copy
		memcpy(screen->frameBuffer+(x+j*screen->width)*4,data+(x+j*screen->width)*4,width*4);

	//Set modified region
	rfbMarkRectAsModified(screen,x,y,width,height);

	//Unlock
	use.DecUse();
}

int VNCServer::End()
{

}

void VNCServer::onMouseEvent(int buttonMask, int x, int y, rfbClientRec* cl)
{
	Debug("-onMouseEvent [mask:%d,x:%d,y:%y]\n",buttonMask,x,y);

	//Get client
	Client *client = (Client *)cl->clientData;
	/*
	//If if has floor
	if (client->GetId()==mouseFloorId)
	{
		if (listener)
			//Send event
			listener->onMouseEvent(buttonMask,x,y);
	}
	 * */
}

void VNCServer::onUpdateDone(rfbClientRec* cl, int result)
{
	//Get lock
	Use* use = (Use*) cl->screen->screenData;

	//Lock
	use->WaitUnusedAndLock();
	
	//If the scren has been resized
	if (cl->scaledScreen!=cl->screen)
	{
		//Get old screen
		rfbScreenInfo* old = cl->scaledScreen;

		//Remove client from it
		old->scaledScreenRefCount--;

		//If old one is no longer used
		if (old && old->scaledScreenRefCount)
		{
			//Free frame buffer
			free(old->frameBuffer);
			//Delete it
			free(old);
		}

		/* setup pseudo scaling */
		cl->scaledScreen = cl->screen;
		cl->scaledScreen->scaledScreenRefCount++;

		//Free region
		sraRgnDestroy(cl->modifiedRegion);

		//Set new modified region
		cl->modifiedRegion = sraRgnCreateRect(0,0,cl->screen->width,cl->screen->height);

		//Frame size has changed
		cl->newFBSizePending = TRUE;

		//Should we update client???
	}

	//Unlock
	use->Unlock();
}


void VNCServer::onOpen(WebSocket *ws)
{

}

void VNCServer::onMessageStart(WebSocket *ws,const WebSocket::MessageType type,const DWORD length)
{
	Debug("-onMessageStart %p size:%d\n",ws,length);

	//Do nothing
}

void VNCServer::onMessageData(WebSocket *ws,const BYTE* data, const DWORD size)
{
	Debug("-onMessageData %p size:%d\n",ws,size);

	//Get client
	Client *client = (Client *)ws->GetUserData();
	//Process it
	client->Process(data,size);
}

void VNCServer::onMessageEnd(WebSocket *ws)
{
	Debug("-onMessageEnd %p\n",ws);
	//Get client
	Client *client = (Client *)ws->GetUserData();
	//Process recevied message
	client->ProcessMessage();
}

void VNCServer::onClose(WebSocket *ws)
{
	 Debug("-onClose %p\n",ws);
}

void VNCServer::onError(WebSocket *ws)
{
	Debug("-onError %p\n",ws);
}

VNCServer::Client::Client(int id,rfbScreenInfo* screen)
{
	//Store id
	this->id = id;

	//No websocket yet
	this->ws = NULL;

	//Create client
	cl = (rfbClientRec*)calloc(sizeof(rfbClientRec),1);

	cl->screen = screen;
	cl->sock = 1;			//Dummy value to allow updatiing
	cl->viewOnly = FALSE;
	/* setup pseudo scaling */
	cl->scaledScreen = screen;
	cl->scaledScreen->scaledScreenRefCount++;

	//Allocate size for name
	cl->host = (char*) malloc(64);
	//Print id
	snprintf(cl->host,64,"%d",id);

	rfbResetStats(cl);
	cl->clientData = this;
	cl->clientGoneHook = rfbDoNothingWithClient;

	cl->state = _rfbClientRec::RFB_PROTOCOL_VERSION;

	cl->reverseConnection = FALSE;
	cl->readyForSetColourMapEntries = FALSE;
	cl->useCopyRect = FALSE;
	cl->preferredEncoding = -1;
	cl->correMaxWidth = 48;
	cl->correMaxHeight = 48;
#ifdef LIBVNCSERVER_HAVE_LIBZ
	cl->zrleData = NULL;
#endif

	cl->copyRegion = sraRgnCreate();
	cl->copyDX = 0;
	cl->copyDY = 0;

	cl->modifiedRegion = sraRgnCreateRect(0,0,screen->width,screen->height);

	INIT_MUTEX(cl->updateMutex);
	INIT_COND(cl->updateCond);

	cl->requestedRegion = sraRgnCreate();

	cl->format = cl->screen->serverFormat;
	cl->translateFn = rfbTranslateNone;
	cl->translateLookupTable = NULL;

	//LOCK(rfbClientListMutex);

	//IF_PTHREADS(cl->refCount = 0);
	//cl->next = screen->clientHead;
	//cl->prev = NULL;
	//if (screen->clientHead)
	//	screen->clientHead->prev = cl;

	//screen->clientHead = cl;
	//UNLOCK(rfbClientListMutex);

#if defined(LIBVNCSERVER_HAVE_LIBZ) || defined(LIBVNCSERVER_HAVE_LIBPNG)
	cl->tightQualityLevel = -1;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	cl->tightCompressLevel = TIGHT_DEFAULT_COMPRESSION;
	cl->turboSubsampLevel = TURBO_DEFAULT_SUBSAMP;
	for (int i = 0; i < 4; i++)
		cl->zsActive[i] = FALSE;
#endif
#endif

	cl->fileTransfer.fd = -1;

	cl->enableCursorShapeUpdates = FALSE;
	cl->enableCursorPosUpdates = FALSE;
	cl->useRichCursorEncoding = FALSE;
	cl->enableLastRectEncoding = FALSE;
	cl->enableKeyboardLedState = FALSE;
	cl->enableSupportedMessages = FALSE;
	cl->enableSupportedEncodings = FALSE;
	cl->enableServerIdentity = FALSE;
	cl->lastKeyboardLedState = -1;
	cl->cursorX = screen->cursorX;
	cl->cursorY = screen->cursorY;
	cl->useNewFBSize = FALSE;

#ifdef LIBVNCSERVER_HAVE_LIBZ
	cl->compStreamInited = FALSE;
	cl->compStream.total_in = 0;
	cl->compStream.total_out = 0;
	cl->compStream.zalloc = Z_NULL;
	cl->compStream.zfree = Z_NULL;
	cl->compStream.opaque = Z_NULL;

	cl->zlibCompressLevel = 5;
#endif

	cl->progressiveSliceY = 0;
	cl->extensions = NULL;
	cl->lastPtrX = -1;
}

VNCServer::Client::~Client()
{
	//Remvoe us from screen
	cl->scaledScreen->scaledScreenRefCount--;
	//Free name
	free(cl->host);
	//Free regions
	sraRgnDestroy(cl->modifiedRegion);
	sraRgnDestroy(cl->copyRegion);
	//Free mem
	free(cl);
}

int VNCServer::Client::Connect(WebSocket *ws)
{
	//Store websocekt
	this->ws = ws;

	rfbProtocolVersionMsg pv;
	sprintf(pv,rfbProtocolVersionFormat,cl->screen->protocolMajorVersion,cl->screen->protocolMinorVersion);

	//Write protocol version
	if (rfbWriteExact(cl, pv, sz_rfbProtocolVersionMsg) < 0)
	{
		rfbLogPerror("rfbNewClient: write");
		rfbCloseClient(cl);
		rfbClientConnectionGone(cl);
		return Error("-Could not write protocol version");
	}

	//Enable extension
	for(rfbProtocolExtension* extension = rfbGetExtensionIterator(); extension; extension=extension->next)
	{
		void* data = NULL;
		/* if the extension does not have a newClient method, it wants
		* to be initialized later. */
		if(extension->newClient && extension->newClient(cl, &data))
			rfbEnableExtension(cl, extension, data);
	}
	rfbReleaseExtensionIterator();

	cl->onHold = FALSE;

	return 1;
}


int VNCServer::Client::Process(const BYTE* data,DWORD size)
{
	Debug("-Process size:%d\n",size);
	//Puss to buffer
	buffer.push(data,size);
	//Signal
	wait.Signal();
	//Return written
	return size;
}

int VNCServer::Client::WaitForData(DWORD usecs)
{
	Debug(">WaitForData %d\n",buffer.length());

	//Lock
	wait.Lock();

	//Wait for data
	int ret = wait.WaitSignal(usecs*1000);

	//Unlock
	wait.Unlock();

	Debug("<WaitForData %d %d\n",buffer.length(),ret);

	return ret;
}

void VNCServer::Client::Close()
{
	//Cancel any read
	CancelWait();
	//Close
	ws->Close();
}

void VNCServer::Client::CancelWait()
{
	wait.Cancel();
}

int VNCServer::Client::Read(char *out, int size, int timeout)
{
	Debug(">Read requested:%d,buffered:%d\n",size,buffer.length());
	//Lock
	wait.Lock();

	//Bytes to read
	int num = size;

	//Check if we have enought data
	while(buffer.length()<num)
	{
		//Wait for mor data
		if (!wait.WaitSignal(timeout))
			//Error
			return 0;
	}

	//Read
	buffer.pop((BYTE*)out,num);

	//Unlock
	wait.Unlock();

	Debug("<Read requested:%d,returned:%d,left:%d\n",size,num,buffer.length());


	//Return readed
	return num;
}
int VNCServer::Client::Write(const char *buf, int size)
{
	//Send data
	ws->SendMessage((BYTE*)buf,size);
	//OK
	return size;
}

int VNCServer::Client::ProcessMessage()
{
	//While we have data
	while(buffer.length())
		//Process message
		rfbProcessClientMessage(cl);
	//Update just in case
	rfbUpdateClient(cl);
	//OK
	return 1;
}

void VNCServer::Client::Update()
{
	//Update just in case
	rfbUpdateClient(cl);
}

void VNCServer::Client::ResizeScreen(rfbScreenInfo* screen)
{
	//Set new one and wait for display finished hook to do the other chantes
	cl->screen = screen;
}
