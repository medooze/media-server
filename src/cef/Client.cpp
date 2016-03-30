/* 
 * File:   Client.cpp
 * Author: Sergio
 * 
 * Created on 2 de julio de 2015, 18:24
 */

#include "Client.h"
#include "log.h"

Client::Client(Listener* listener)
{ 
	//Store
	this->listener = listener;
}

Client::~Client()
{
}

bool Client::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
	Log("-GetViewRect browser:%p host:%p\n",browser.get(),browser.get()->GetHost().get());
	//Call listener
	return listener && listener->GetViewRect(rect);
}

void Client::OnPaint(CefRefPtr<CefBrowser>, CefRenderHandler::PaintElementType type, const RectList& rects, const void* buffer, int width, int height)
{
	//Call listener
	if (listener) listener->OnPaint(type,rects,(const BYTE*)buffer,width,height);
}
	
//Called only from Browser
void Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) 
{
	 Log("-BrowserCreated browser:%p host:%p\n",browser.get(),browser.get()->GetHost().get());
	//Set focus
	browser->GetHost()->SetFocus(true);
	//Call listener
	if (listener) listener->OnBrowserCreated(browser);
}

void Client::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	 Log("-BrowserDestroyed browser:%p host:%p\n",browser.get(),browser.get()->GetHost().get());
	//Call Listener
	if (listener) listener->OnBrowserDestroyed(browser);
}

///
// Called when the browser wants to show or hide the popup widget. The popup
// should be shown if |show| is true and hidden if |show| is false.
void Client::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) 
{
	Log("-PopupShow  browser:%p show:%d\n",browser.get(),show);
	//Call Listener
	if (listener) listener->OnPopupShow(show);
}

///
// Called when the browser wants to move or resize the popup widget. |rect|
// contains the new location and size in view coordinates.
void Client::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) 
{
	Log("-PopupSize  browser:%p rect:[%d,%d,%d,%d]\n",browser.get(),rect.x,rect.y,rect.width,rect.height);
	//Call Listener
	if (listener) listener->OnPopupSize(rect);
}	


