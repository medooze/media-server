#ifndef _RTMP_APPLICATION_H_
#define _RTMP_APPLICATION_H_
#include "config.h"
#include "rtmpnetconnection.h"
#include <string>

class RTMPApplication
{
public:
	virtual RTMPNetConnection* Connect(const std::wstring& appName,RTMPNetConnection::Listener *listener) = 0;
};

#endif
