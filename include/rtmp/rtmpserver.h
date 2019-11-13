#ifndef _RTMPSERVER_H_
#define _RTPMSERVER_H_
#include <memory>
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
	virtual ~RTMPServer();

	int Init(int port);
	int AddApplication(const wchar_t* name,RTMPApplication *app);
	int End();
	
	/** Listener for RTMPConnection */
	virtual RTMPNetConnection* OnConnect(const std::wstring& appName,RTMPNetConnection::Listener *listener,std::function<void(bool)> accept) override;
	virtual void onDisconnect(RTMPConnection *con) override;

protected:
        int Run();

private:
        static void * run(void *par);

	void CreateConnection(int fd);
	void DeleteAllConnections();

private:
	int inited;
	int serverPort;
	int server;

	std::map<int,RTMPConnection::shared> connections;
	std::map<std::wstring,RTMPApplication *> applications;
	pthread_t serverThread;
	pthread_mutex_t	sessionMutex;
};

#endif
