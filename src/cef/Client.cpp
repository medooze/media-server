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
	
