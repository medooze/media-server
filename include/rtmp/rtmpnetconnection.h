/* 
 * File:   RTMPNetConnection.h
 * Author: Sergio
 *
 * Created on 19 de junio de 2012, 14:26
 */

#ifndef RTMPNETCONNECTION_H
#define	RTMPNETCONNECTION_H

#include <memory>
#include "rtmpstream.h"

class RTMPNetConnection
{
public:
	using shared = std::shared_ptr<RTMPNetConnection>;
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onNetConnectionStatus(QWORD transId, const RTMPNetStatusEventInfo &info,const wchar_t *message) = 0;
		virtual void onNetConnectionDisconnected() = 0;
	};


public:
	virtual ~RTMPNetConnection();
	virtual void AddListener(Listener* listener);
	virtual void RemoveListener(Listener* listener);
	virtual void SendStatus(QWORD transId, const RTMPNetStatusEventInfo &info,const wchar_t *message);
	virtual void Disconnect();
	
	/* Interface */
	virtual RTMPNetStream::shared CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener *listener) = 0;
	virtual void DeleteStream(const RTMPNetStream::shared& stream) = 0;
	virtual void Disconnected();
	
protected:
	int RegisterStream(const RTMPNetStream::shared& stream);
	int UnRegisterStream(const RTMPNetStream::shared& stream);
protected:
	std::set<Listener*>			listeners;
	std::map<DWORD,RTMPNetStream::shared>	streams;
	Mutex					listenerLock;
	Mutex					streamLock;
};

#endif	/* RTMPNETCONNECTION_H */

