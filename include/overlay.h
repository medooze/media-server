/* 
 * File:   overlay.h
 * Author: Sergio
 *
 * Created on 29 de marzo de 2012, 23:17
 */

#ifndef OVERLAY_H
#define	OVERLAY_H
#include "config.h"

class Overlay
{
public:
	Overlay(DWORD width,DWORD height);
	~Overlay();
	int LoadPNG(const char* png);
	int LoadSVG(const char* svg);
	int RenderText(const std::wstring& text,DWORD x,DWORD y,DWORD width,DWORD height);
	BYTE* Display(BYTE* frame);
	BYTE* GetOverlay() { return overlay; }
private:
	DWORD overlaySize;
	BYTE* overlay;
	DWORD imageSize;
	BYTE* image;

	DWORD width;
	DWORD height;
	bool display;
};

#endif	/* OVERLAY_H */

