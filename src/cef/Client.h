/* 
 * File:   Client.h
 * Author: Sergio
 *
 * Created on 2 de julio de 2015, 18:24
 */

#ifndef CLIENT_H
#define	CLIENT_H

#define CEF

#include "include/cef_client.h"

class Client : 
	public CefClient,
	public CefRenderHandler
{
public:
	class Listener {
		virtual bool GetViewRect(CefRect& rect) = 0;
		virtual void OnPaint(CefRenderHandler::PaintElementType type, const RectList& rects, const void* buffer, int width, int height)  = 0;
	};
public:
	Client(Listener *listener);
	virtual ~Client();
	
	//Overrride
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() {
		// Return the handler for off-screen rendering events.
		return this;
	}
	
	virtual bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
	virtual void OnPaint(CefRefPtr<CefBrowser> browser, CefRenderHandler::PaintElementType type, const RectList& rects, const void* buffer, int width, int height);
	

	IMPLEMENT_REFCOUNTING(Client);
private:
	Listener* listener;
};

#endif	/* CLIENT_H */

