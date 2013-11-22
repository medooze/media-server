/* 
 * File:   websockets.h
 * Author: Sergio
 *
 * Created on 3 de enero de 2013, 12:40
 */

#ifndef WEBSOCKETS_H
#define	WEBSOCKETS_H

class WebSocket
{
public:
	enum MessageType { Text = 0, Binary = 1 };
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onOpen(WebSocket *ws) = 0;
		virtual void onMessageStart(WebSocket *ws,const WebSocket::MessageType type) = 0;
		virtual void onMessageData(WebSocket *ws,const BYTE* data, const DWORD size) = 0;
		virtual void onMessageEnd(WebSocket *ws) = 0;
		virtual void onError(WebSocket *ws) = 0;
		virtual void onClose(WebSocket *ws) = 0;
	};
public:
	virtual void Accept(Listener *listener) = 0;
	virtual void Reject(const WORD code, const char* reason) = 0;
	virtual void SendMessage(const std::string& message) = 0;
	virtual void SendMessage(const BYTE* data, const DWORD size) = 0;
	virtual void Close() = 0;

	virtual void* GetUserData()			{ return userData;		}
	virtual void  SetUserData(void* userData)	{ this->userData = userData;	}
private:
	void *userData;
};

#endif	/* WEBSOCKETS_H */

