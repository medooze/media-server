/* 
 * File:   overlay.h
 * Author: Sergio
 *
 * Created on 29 de marzo de 2012, 23:17
 */

#ifndef OVERLAY_H
#define	OVERLAY_H
#include "config.h"


class Canvas
{
public:
	Canvas(DWORD width,DWORD height);
	~Canvas();
	int LoadPNG(const char* png);
	int LoadSVG(const char* svg);
	int RenderText(const std::wstring& text,DWORD x,DWORD y,DWORD width,DWORD height);
	int RenderText(const std::wstring& text,DWORD x,DWORD y,DWORD width,DWORD height,const Properties& properties);
	int RenderText(const std::string& utf8,DWORD x,DWORD y,DWORD width,DWORD height,const Properties& properties);
	void Draw(BYTE*image, BYTE* frame);
	void Reset();
	BYTE* GetCanvas()	{ return overlay;	}
protected:
	DWORD overlaySize;
	BYTE* overlay;
	DWORD width;
	DWORD height;
	bool display;	
};

class Overlay : public Canvas
{
public:
	Overlay(DWORD width,DWORD height);
	~Overlay();

	BYTE* Display(BYTE* frame);
	BYTE* GetOverlay() { return GetCanvas(); }
private:
	DWORD imageSize;
	BYTE* image;
};


#endif	/* OVERLAY_H */

