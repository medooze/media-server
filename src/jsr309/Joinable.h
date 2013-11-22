/* 
 * File:   Joinable.h
 * Author: Sergio
 *
 * Created on 7 de septiembre de 2011, 0:59
 */

#ifndef JOINABLE_H
#define	JOINABLE_H
#include "rtp.h"

class Joinable
{
public:
	class Listener 
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onRTPPacket(RTPPacket &packet) = 0;
		virtual void onResetStream() = 0;
		virtual void onEndStream() = 0;
	};
public:
	virtual void AddListener(Listener *listener) = 0;
	virtual void Update() = 0;
	virtual void SetREMB(DWORD estimation) = 0;

	virtual void RemoveListener(Listener *listener) = 0;
};


#endif	/* JOINABLE_H */

