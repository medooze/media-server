#ifndef _RTMP_APPLICATION_H_
#define _RTMP_APPLICATION_H_
#include "config.h"
#include "rtmpnetconnection.h"
#include <string>
#include <functional>
#include <sys/socket.h>

class RTMPApplication
{
public:
	virtual RTMPNetConnection::shared Connect(
		const struct sockaddr_in& peername,
		const std::wstring& appName,
		RTMPNetConnection::Listener *listener,
		std::function<void(bool)> accept
	) = 0;
};

#endif
