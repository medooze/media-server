/* 
 * File:   Client.h
 * Author: Sergio
 *
 * Created on 2 de julio de 2015, 18:24
 */

#ifndef CLIENT_H
#define	CLIENT_H

#include "include/cef_client.h"

class Client : 
	public CefClient,
	public CefRenderHandler
{
public:
	class Listener {
		
	};
public:
	Client();
	virtual ~Client();
	
	//Overrride
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() {
		// Return the handler for off-screen rendering events.
		return this;
	}
	
	virtual bool GetViewRect(CefRefPtr<CefBrowser>, CefRect&);
	virtual void OnPaint(CefRefPtr<CefBrowser>, CefRenderHandler::PaintElementType, const RectList&, const void*, int, int);
	

	IMPLEMENT_REFCOUNTING(Client);
};

#endif	/* CLIENT_H */

