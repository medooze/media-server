#ifndef _RTMPSERVER_H_
#define _RTPMSERVER_H_
#include "pthread.h"
#include "rtmpstream.h"
#include "rtmpapplication.h"
#include "rtmpconnection.h"
#include <list>


class RTMPServer : public RTMPConnection::Listener
{
public:
	/** Constructors */
	RTMPServer();
	~RTMPServer();

	int Init(int port);
	int AddApplication(const wchar_t* name,RTMPApplication *app);
	int Start();
	int Stop();
	int End();
	
	/** Listener for RTMPConnection */
	RTMPNetConnection* OnConnect(const std::wstring& appName,RTMPNetConnection::Listener *listener);
	void onDisconnect(RTMPConnection *con);

protected:
        int Run();

private:
	typedef std::map<std::wstring,RTMPApplication *> ApplicationMap;
	typedef std::list<RTMPConnection*>	Connections;

        static void * run(void *par);

	void CreateConnection(int fd);
	void CleanZombies();
	void DeleteAllConnections();

private:
	int inited;
	int serverPort;
	int server;

	Connections connections;
	Connections zombies;
	ApplicationMap applications;
	pthread_t serverThread;
	pthread_mutex_t	sessionMutex;
};

#endif
