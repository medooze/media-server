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
	enum State
	{
		Initial,
		Checking,
		Connected,
		TimedOut
	};
	class Listener
	{
	public:
		virtual int onData(const ICERemoteCandidate* candidate,const BYTE* data,DWORD size) = 0;
	};
public:
	
	ICERemoteCandidate(const std::string& ip,const WORD port,Listener *listener) :
		listener(listener)
	{
		//Set values
		addr.sin_family		= AF_INET;
		addr.sin_port		= htons(port);
		addr.sin_addr.s_addr	= inet_addr(ip.c_str());
	}
	
	ICERemoteCandidate(const char *ip,const WORD port,Listener *listener) :
		listener(listener)
	{
		//Set values
		addr.sin_family		= AF_INET;
		addr.sin_port		= htons(port);
		addr.sin_addr.s_addr	= inet_addr(ip);
	}
	
	ICERemoteCandidate(const DWORD ipAddr ,const WORD port,Listener *listener) :
		listener(listener)
	{
		//Set values
		addr.sin_family		= AF_INET;
		addr.sin_port		= htons(port);
		addr.sin_addr.s_addr	= htonl(ipAddr);
	}
	
	int onData(const BYTE* data, DWORD size)
	{
		return listener->onData(this,data,size);
	}
	void SetState(State state) 
	{
		this->state = state;
	}
	
	const sockaddr* GetAddress()		const { return (const sockaddr*)&addr;		}
	      DWORD     GetAddressLen()		const { return sizeof(sockaddr_in);		}
	const char*	GetIP()			const { return inet_ntoa(addr.sin_addr);	}
	      DWORD     GetIPAddress()		const { return ntohl(addr.sin_addr.s_addr);	}
	      WORD	GetPort()		const {	return ntohs(addr.sin_port);		}
	std::string	GetRemoteAddress()	const { return std::string(GetIP()) + ":" + std::to_string(GetPort()); }
	State		GetState()		const { return state;				}
public:
	static std::string GetRemoteAddress(DWORD address,WORD port)
	{
		const uint8_t* host = (const uint8_t*)&address;
		return  std::to_string(get1(host,3)) + "." +
			std::to_string(get1(host,2)) + "." +
			std::to_string(get1(host,1)) + "." +
			std::to_string(get1(host,0)) + ":" +
			std::to_string(port);
	}
private:
	State state		= Initial;
	sockaddr_in addr	= {};
	Listener *listener;
};


#endif /* ICEREMOTECANDIDATE_H */

