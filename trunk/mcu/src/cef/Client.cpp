/* 
 * File:   Client.cpp
 * Author: Sergio
 * 
 * Created on 2 de julio de 2015, 18:24
 */

#include "Client.h"

Client::Client()
{ 
}


Client::~Client()
{
}

bool Client::Client::GetViewRect(CefRefPtr<CefBrowser>, CefRect&)
{
	return true;
}
void Client::OnPaint(CefRefPtr<CefBrowser>, CefRenderHandler::PaintElementType, const RectList&, const void*, int, int)
{

}
	