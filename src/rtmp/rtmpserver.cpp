#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/poll.h>
#include <fcntl.h>
#include "tools.h"
#include "log.h"
#include "assertions.h"
#include "rtmp/rtmpserver.h"

/************************
* RTMPServer
* 	Constructor
*************************/
RTMPServer::RTMPServer()
{
	//Y no tamos iniciados
	inited = 0;
	serverPort = 0;
	server = FD_INVALID;

	//Create mutx
	pthread_mutex_init(&sessionMutex,0);
}


/************************
* ~ RTMPServer
* 	Destructor
*************************/
RTMPServer::~RTMPServer()
{
	//Check we have been correctly ended
	if (inited)
		//End it anyway
		End();
	//Destroy mutex
	pthread_mutex_destroy(&sessionMutex);
}

/************************
* Init
* 	Open the listening server port
*************************/
int RTMPServer::Init(int port)
{
	Log("-RTMPServer::Init() [port:%d]\n",port);
	
	//Check not already inited
	if (inited)
		//Error
		return Error("-RTMPServer::Init() RTMP Server is already running.\n");

	

	//Save server port
	serverPort = port;

	//I am inited
	inited = 1;

	//Create threads
	createPriorityThread(&serverThread,run,this,0);

	//Return ok
	return 1;
}

/***************************
 * Run
 * 	Server running thread
 ***************************/
int RTMPServer::Run()
{
	sockaddr_in addr;
	pollfd ufds[1];

init:
	//Log
	Log(">RTMPServer::Run() [%p]\n",this);
	//Create socket
	server = socket(AF_INET, SOCK_STREAM, 0);

	//Set SO_REUSEADDR on a socket to true (1):
	int optval = 1;
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	//Bind to first available port
	memset(&addr,0,sizeof(addr));
	addr.sin_family 	= AF_INET;
	addr.sin_addr.s_addr 	= INADDR_ANY;
	addr.sin_port 		= htons(serverPort);

	//Bind
     	if (bind(server, (sockaddr *) &addr, sizeof(addr)) < 0)
		//Error
		return Error("-RTMPServer::Run() Can't bind server socket. errno = %d.\n", errno);

	//Listen for connections
	if (listen(server,5)<0)
		//Error
		return Error("-RTMPServer::Run() Can't listen on server socket. errno = %d\n", errno);

	//Set values for polling
	ufds[0].fd = server;
	ufds[0].events = POLLIN | POLLHUP | POLLERR ;

	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(server,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	fcntl(server,F_SETFL,fsflags);

	//Run until ended
	while(inited)
	{
		//Log
		Log("-RTMPServer::Run() Server accepting connections [fd:%d]\n", ufds[0].fd);

		//Wait for events
		if (poll(ufds,1,-1)<0)
		{
			//Error
			Error("-RTMPServer::Run() pool error [fd:%d,errno:%d]\n",ufds[0].fd,errno);
			//Check if already inited
			if (!inited)
				//Exit
				break;
			//Close socket just in case
			MCU_CLOSE(server);
			//Invalidate
			server = FD_INVALID;
			//Re-init
			goto init;
		}

		//Chek events, will fail if closed by End() so we can exit
		if (ufds[0].revents!=POLLIN)
		{
			//Error
			Error("-RTMPServer::Run() poolin error event [event:%d,fd:%d,errno:%d]\n",ufds[0].revents,ufds[0].fd,errno);
			//Check if already inited
			if (!inited)
				//Exit
				break;
			//Close socket just in case
			MCU_CLOSE(server);
			//Invalidate
			server = FD_INVALID;
			//Re-init
			goto init;
		}

		//Accpept incoming connections
		int fd = accept(server,NULL,0);

		//If error
		if (fd<0)
		{
			//LOg error
			Error("-RTMPServer::Run() error accepting new connection [fd:%d,errno:%d]\n",server,errno);
			//Check if already inited
			if (!inited)
				//Exit
				break;
			//Close socket just in case
			MCU_CLOSE(server);
			//Invalidate
			server = FD_INVALID;
			//Re-init
			goto init;
		}

		//Set non blocking again
		fsflags = fcntl(fd,F_GETFL,0);
		fsflags |= O_NONBLOCK;
		fcntl(fd,F_SETFL,fsflags);

		//Create the connection
		CreateConnection(fd);
	}

	Log("<RTMPServer::Run()\n");

	return 0;
}

/*************************
 * CreateConnection
 * 	Create new RTMP Connection for socket
 *************************/
void RTMPServer::CreateConnection(int fd)
{
	//Create new RTMP connection
	auto rtmp = std::make_shared<RTMPConnection>(this);

	Log(">RTMPServer::CreateConnection() connection [fd:%d,%p]\n",fd,rtmp.get());

	//Init connection
	rtmp->Init(fd);

	//Lock list
	pthread_mutex_lock(&sessionMutex);

	//Append
	connections[fd] = rtmp;

	//Unlock
	pthread_mutex_unlock(&sessionMutex);

	Log("<RTMPServer::CreateConnection() [0x%x]\n",rtmp.get());
}

/*********************
 * DeleteAllConnections
 *	End all connections and clean list
 *********************/
void RTMPServer::DeleteAllConnections()
{
	Log(">RTMPServer::DeleteAllConnections()\n");

	//Lock list
	pthread_mutex_lock(&sessionMutex);

	//Clear all connections
	connections.clear();
	
	//Unlock list
	pthread_mutex_unlock(&sessionMutex);
	

	Log("<RTMPServer::DeleteAllConnections()\n");

}

/***********************
* run
*       Helper thread function
************************/
void * RTMPServer::run(void *par)
{
        Log("-RTMP Server Thread [%p]\n",pthread_self());

        //Obtenemos el parametro
        RTMPServer *ses = (RTMPServer *)par;

        //Bloqueamos las señales
        blocksignals();

        //Ejecutamos
        ses->Run();
	//Exit
	return NULL;
}


/************************
* End
* 	End server and close all connections
*************************/
int RTMPServer::End()
{
	Log(">RTMPServer::End()\n");

	//Check we have been inited
	if (!inited)
		//Do nothing
		return 0;

	//Stop thread
	inited = 0;

	//Close server socket
	shutdown(server,SHUT_RDWR);
	//Will cause poll function to exit
	MCU_CLOSE(server);
	//Invalidate
	server = FD_INVALID;

	//Wait for server thread to close
        Log("-RTMPServer::End() Joining server thread [%d,%d]\n",serverThread,inited);
        pthread_join(serverThread,NULL);
        Log("-RTMPServer::End() Joined server thread [%d]\n",serverThread);

	//Delete connections
	DeleteAllConnections();

	Log("<RTMPServer::End()\n");
	
	return 1;
}

/**********************************
 * AddApplication
 *   Set a handler for an application
 *********************************/
int RTMPServer::AddApplication(const wchar_t* name,RTMPApplication *app)
{
	Log("-RTMPServer::AddApplication() [name:%ls]\n",name);
	//Store
	applications[std::wstring(name)] = app;

	//Exit
	return 1;
}

/**************************************
 * OnConnect
 *   Event launched from RTMPConnection to indicate a net connection stream
 *   Should return the RTMPStream associated to the url
 *************************************/
RTMPNetConnection* RTMPServer::OnConnect(const std::wstring &appName,RTMPNetConnection::Listener *listener,std::function<void(bool)> accept)
{
	//Recorremos la lista
	for (auto it=applications.begin(); it!=applications.end(); ++it)
	{
		//Si la uri empieza por la base del handler
		if (appName.find(it->first)==0)
			//Ejecutamos el handler
			return it->second->Connect(appName,listener,accept);
	}

	//Not found
	return NULL;
}

/**************************************
 * OnDisconnect
 *   Event launched from RTMPConnection to indicate that the connection stream has been disconnected
  *************************************/
void RTMPServer::onDisconnect(RTMPConnection *con)
{
	Log("-RTMPServer::onDisconnect() [%p,socket:%d]\n",con,con->GetSocket());

	//Lock list
	pthread_mutex_lock(&sessionMutex);

	//Remove from list
	connections.erase(con->GetSocket());

	//Unlock list
	pthread_mutex_unlock(&sessionMutex);
}
