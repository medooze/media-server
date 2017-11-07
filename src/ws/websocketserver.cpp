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
#include "ws/websocketserver.h"

/************************
* WebSocketServer
* 	Constructor
*************************/
WebSocketServer::WebSocketServer()
{
	//Y no tamos iniciados
	inited = 0;
	serverPort = 0;
	server = FD_INVALID;

	//Create mutx
	pthread_mutex_init(&sessionMutex,0);
}


/************************
* ~ WebSocketServer
* 	Destructor
*************************/
WebSocketServer::~WebSocketServer()
{
	//Check we have been correctly ended
	if (inited)
		//End it anyway
		End();
	//Destroy mutex
	pthread_mutex_destroy(&sessionMutex);
}

void WebSocketServer::AddHandler(const std::string base,Handler* hnd)
{
	Log("-WebSocket handler on %s\n",base.c_str());

	//A�adimos al map
	handlers[base] = hnd;
}

/************************
* Init
* 	Open the listening server port
*************************/
int WebSocketServer::Init(int port)
{
	//Check not already inited
	if (inited)
		//Error
		return Error("-Init: WebSocket Server is already running.\n");

	Log("-Init WebSocket Server [%d]\n",port);

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
int WebSocketServer::Run()
{
	sockaddr_in addr;
	pollfd ufds[1];

init:
	//Log
	Log(">Run WebSocket Server [%p,port:%d]\n",this,serverPort);
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
		return Error("Can't bind server socket. errno = %d.\n", errno);

	//Listen for connections
	if (listen(server,5)<0)
		//Error
		return Error("Can't listen on server socket. errno = %d\n", errno);

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
		Log("-WebSocket Server accepting connections [fd:%d]\n", ufds[0].fd);

		//Clean zombies each time
		CleanZombies();

		//Wait for events
		if (poll(ufds,1,-1)<0)
		{
			//Error
			Error("WebSocketServer: pool error [fd:%d,errno:%d]\n",ufds[0].fd,errno);
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
			Error("WebSocketServer: poolin error event [event:%d,fd:%d,errno:%d]\n",ufds[0].revents,ufds[0].fd,errno);
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
			Error("WebSocketServer: error accepting new connection [fd:%d,errno:%d]\n",server,errno);
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

	Log("<Run WebSocket Server\n");

	return 0;
}

/*************************
 * CreateConnection
 * 	Create new WebSocket Connection for socket
 *************************/
void WebSocketServer::CreateConnection(int fd)
{
	Log(">Creating connection\n");

	//Create new WebSocket connection
	WebSocketConnection* con = new WebSocketConnection(this);

	Log("-Incoming connection [%d,%p]\n",fd,con);

	//Init connection
	con->Init(fd);

	Log("<Creating connection [0x%x]\n",con);
}

/**************************
 * DeleteConnection
 * 	DeleteConnection
 **************************/
void WebSocketServer::CleanZombies()
{
	//Lock list
	pthread_mutex_lock(&sessionMutex);

	//Zombie iterator
	for (Connections::iterator it=zombies.begin();it!=zombies.end();++it)
	{
		//Get connection
		WebSocketConnection *con = *it;
		//Delete connection
		delete con;
	}

	//Clear zombies
	zombies.clear();

	//Unlock list
	pthread_mutex_unlock(&sessionMutex);

}

/***********************
* run
*       Helper thread function
************************/
void * WebSocketServer::run(void *par)
{
        Log("-WebSocket Server Thread [%d]\n",pthread_self());

        //Obtenemos el parametro
        WebSocketServer *ses = (WebSocketServer *)par;

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
int WebSocketServer::End()
{
	Log(">End WebSocket Server\n");

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
        Log("Joining server thread [%d,%d]\n",serverThread,inited);
        pthread_join(serverThread,NULL);
        Log("Joined server thread [%d]\n",serverThread);

	Log("<End WebSocket Server\n");
}

void WebSocketServer::onUpgradeRequest(WebSocketConnection* conn)
{
	//Get request
	HTTPRequest *request = conn->GetRequest();
	//Get URL
	std::string uri = request->GetRequestURI();
	//Fore each registere handler in reverse order
	for (Handlers::reverse_iterator it=handlers.rbegin();it!=handlers.rend();it++)
	{
		//Si la uri empieza por la base del handler
		if (uri.find((*it).first)==0)
		{
			//Ejecutamos el handler
			it->second->onWebSocketConnection(*request,(WebSocket*)conn);
			//Found
			return;
		}
	}
	//reject it
	conn->Reject(404,"No handlers for that url found");
}

void WebSocketServer::onDisconnected(WebSocketConnection* conn)
{
	//Lock list
	pthread_mutex_lock(&sessionMutex);
	//Add to zombie
	zombies.push_back(conn);
	//Unlock list
	pthread_mutex_unlock(&sessionMutex);
}
