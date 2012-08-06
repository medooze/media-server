/* 
 * File:   RTPEndpoint.h
 * Author: Sergio
 *
 * Created on 7 de septiembre de 2011, 12:16
 */

#ifndef RTPENDPOINT_H
#define	RTPENDPOINT_H

#include "rtpsession.h"
#include "RTPMultiplexer.h"
#include "Joinable.h"




class RTPEndpoint :
	public RTPSession,
	public RTPMultiplexer,
	public Joinable::Listener,
	public RTPSession::Listener
{
public:
	RTPEndpoint(MediaFrame::Type type);
	virtual ~RTPEndpoint();

	int Init();
	int RequestUpdate();
	int StartReceiving();
	int StopReceiving();
	int StartSending();
	int StopSending();
	int End();

	MediaFrame::Type GetType() { return type; }

	//Attach/Dettach to joinables
	int Attach(Joinable *join);
	int Dettach();

	//Joinable::Listener
	virtual void onRTPPacket(RTPPacket &packet);
	virtual void onResetStream();
	virtual void onEndStream();

	//RTPSession::Listener
	virtual void onFPURequested(RTPSession *session);
	
protected:
	int RunAudio();
	int RunVideo();
	int RunText();

private:
	//Funciones propias
	static void *run(void *par);

private:
	Joinable *joined;
	MediaFrame::Type type;
	pthread_t thread;
	DWORD codec;
	DWORD timestamp;
	DWORD freq;
	timeval prev;
	DWORD prevts;
	bool inited;
	bool reseted;
	bool sending;
	bool receiving;

	

};

#endif	/* RTPENDPOINT_H */

