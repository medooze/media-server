/* 
 * File:   RTMPNetConnection.h
 * Author: Sergio
 *
 * Created on 19 de junio de 2012, 14:26
 */

#ifndef RTMPNETCONNECTION_H
#define	RTMPNETCONNECTION_H
#include "rtmpstream.h"

class RTMPNetConnection
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onNetConnectionStatus(const RTMPNetStatusEventInfo &info,const wchar_t *message) = 0;
		virtual void onNetConnectionDisconnected() = 0;
	};
public:
	virtual ~RTMPNetConnection();
	virtual void Connect(Listener* listener);
	virtual void Disconnect(Listener* listener);

	/* Interface */
	virtual RTMPNetStream* CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener *listener) = 0;
	virtual void DeleteStream(RTMPNetStream *stream) = 0;
protected:
	int RegisterStream(RTMPNetStream* stream);
	int UnRegisterStream(RTMPNetStream* stream);
	void fireOnNetConnectionDisconnected();
protected:
	typedef std::set<Listener*> Listeners;
	typedef std::set<RTMPNetStream*> RTMPNetStreams;
protected:
	Listeners	listeners;
	RTMPNetStreams	streams;
	Use		lock;
};

#endif	/* RTMPNETCONNECTION_H */

