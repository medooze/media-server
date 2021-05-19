#ifndef _RTMP_APPLICATION_H_
#define _RTMP_APPLICATION_H_
#include "config.h"
#include "rtmpnetconnection.h"
#include <string>
#include <functional>

class RTMPApplication
{
public:
	virtual RTMPNetConnection::shared Connect(const std::wstring& appName,RTMPNetConnection::Listener *listener,std::function<void(bool)> accept) = 0;
};

#endif
