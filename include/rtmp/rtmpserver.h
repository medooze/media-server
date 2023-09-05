#ifndef _RTMPSERVER_H_
#define _RTPMSERVER_H_
#include <memory>
#include "pthread.h"
#include "use.h"
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
	virtual RTMPNetConnection::shared OnConnect(
		const struct sockaddr_in& peername,
		const std::wstring& appName,
		RTMPNetConnection::Listener *listener,
		std::function<void(bool)> accept
	) override;
	virtual void onDisconnect(const RTMPConnection::shared& con) override;

protected:
	int Run();

private:
	static void * run(void *par);

	void CreateConnection(int fd);
	void DeleteAllConnections();

	int BindServer();

private:
	int inited = 0;
	int serverPort = 0;
	int server = FD_INVALID;

	std::set<RTMPConnection::shared> connections;
	std::map<std::wstring,RTMPApplication *> applications;
	pthread_t serverThread = 0;
	Mutex mutex;
};

#endif
