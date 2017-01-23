/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Participant.h
 * Author: Sergio
 *
 * Created on 11 de enero de 2017, 14:56
 */

#ifndef SFU_PARTICIPANT_H
#define SFU_PARTICIPANT_H
#include "ICERemoteCandidate.h"
#include "DTLSICETransport.h"
#include <set>

namespace SFU
{
class Stream
{
public:
	class Listener
	{
	public:
		virtual void onMedia(Stream* stream,RTPTimedPacket* packet) = 0;
	};
public:
	std::string msid;
	DWORD audio;
	DWORD rtx;
	DWORD video;
	DWORD fec;
public:
	//RTPIncomingSourceGroup* createOutoginSourceGroup(MediaFrame::Type type);
};


	
class Participant :
	public RTPIncomingSourceGroup::Listener,
	public Stream::Listener
{
public:
	
public:
	Participant(DTLSICETransport* transport);
	virtual ~Participant();
	
	void AddListener(Stream::Listener *listener);
	void RemoveListener(Stream::Listener *listener);
	
	bool AddLocalStream(Stream* stream);
	bool AddRemoteStream(Properties &prop);
	virtual void onRTP(RTPIncomingSourceGroup* group,RTPTimedPacket* packet);
	virtual void onMedia(Stream* stream,RTPTimedPacket* packet);
	Stream* GetStream() { return mine; }
private:
	typedef std::set<Stream::Listener*> Listeners;
	typedef std::set<Stream*> Streams;
private:
	std::string msid;
	Listeners listeners;
	DTLSICETransport* transport;
	RTPIncomingSourceGroup *audio;
	RTPIncomingSourceGroup *video;
	Stream* mine;
	Streams others;
	RTPDepacketizer* d;
	
	
};


}
#endif /* SFU_PARTICIPANT_H */

