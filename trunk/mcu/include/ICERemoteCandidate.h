/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ICERemoteCandidate.h
 * Author: Sergio
 *
 * Created on 11 de enero de 2017, 9:32
 */

#ifndef ICEREMOTECANDIDATE_H
#define ICEREMOTECANDIDATE_H

#include <string>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <srtp2/srtp.h>
#include "config.h"


class ICERemoteCandidate
{
public:
	class Listener
	{
	public:
		virtual int onData(const ICERemoteCandidate* candidate,BYTE* data,DWORD size) = 0;
	};
public:
	ICERemoteCandidate(const char *ip,const WORD port,Listener *listener)
	{
		//Store transport
		this->listener = listener;
		//Rset addr
		memset(&addr,0,sizeof(sockaddr_in));
		
		//Set values
		addr.sin_family		= AF_INET;
		addr.sin_port		= htons(port);
		addr.sin_addr.s_addr	= inet_addr(ip);
	}
	
	int onData(BYTE* data, DWORD size)
	{
		return listener->onData(this,data,size);
	}
	
	const sockaddr* GetAddress()	const	{ return (const sockaddr*)&addr;	}
	const DWORD     GetAddressLen() const	{ return sizeof(sockaddr_in);	}

private:
	sockaddr_in addr;
	Listener *listener;
};


#endif /* ICEREMOTECANDIDATE_H */

