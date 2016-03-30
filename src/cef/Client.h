/* 
 * File:   Client.h
 * Author: Sergio
 *
 * Created on 2 de julio de 2015, 18:24
 */

#ifndef CLIENT_H
#define	CLIENT_H

#include <include/cef_client.h>
#include <include/cef_render_handler.h>
#include "config.h"
#include "log.h"

class Client : 
	public CefClient,
	public CefRenderHandler,
	public CefLifeSpanHandler
{
public:
	class Listener {
	public: 
		virtual void OnBrowserCreated(CefRefPtr<CefBrowser> browser) = 0;
		virtual bool GetViewRect(CefRect& rect) = 0;
		virtual void OnPaint(CefRenderHandler::PaintElementType type, const CefRenderHandler::RectList& rects, const BYTE* buffer, int width, int height)  = 0;
		virtual void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) = 0;
		virtual void OnPopupShow(bool show) = 0;
		virtual void OnPopupSize(const CefRect& rect) = 0;
	};
public:
	Client(Listener *listener);
	virtual ~Client();
	
	//Overrride
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() {
		// Return the handler for off-screen rendering events.
		return this;
	}
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() {
		// Return browser life span handler
		return this;
	}
	
	virtual bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
	virtual void OnPaint(CefRefPtr<CefBrowser> browser, CefRenderHandler::PaintElementType type, const RectList& rects, const void* buffer, int width, int height);

	//Called only from Browser
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser);
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser);

	///
	// Called when the browser wants to show or hide the popup widget. The popup
	// should be shown if |show| is true and hidden if |show| is false.
	virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show);
	///
	// Called when the browser wants to move or resize the popup widget. |rect|
	// contains the new location and size in view coordinates.
	virtual void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect);


	IMPLEMENT_REFCOUNTING(Client);
private:
	Listener* listener;
};

#endif	/* CLIENT_H */

