/* 
 * File:   RTPMultiplexer.h
 * Author: Sergio
 *
 * Created on 7 de septiembre de 2011, 12:19
 */

#ifndef RTPMULTIPLEXER_H
#define	RTPMULTIPLEXER_H
#include <set>
#include "config.h"
#include "rtp.h"
#include "Joinable.h"

class RTPMultiplexer :
	public Joinable
{
public:
	RTPMultiplexer();
	virtual ~RTPMultiplexer();

	void Multiplex(RTPPacket &packet);
	void ResetStream();
	void EndStream();
	
	//Joinable interface
	virtual void AddListener(Listener *listener);
	virtual void Update();
	virtual void SetREMB(DWORD estimation);
	virtual void RemoveListener(Listener *listener);
private:
	typedef std::set<Joinable::Listener*> Listeners;
protected:
	Listeners	listeners;
	pthread_mutex_t mutex;
};

#endif	/* RTPMULTIPLEXER_H */

