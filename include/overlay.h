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
	int LoadPNG(const char*);
	int GenerateSVG(const char*);
	BYTE* Display(BYTE* frame);
	BYTE* GetOverlay() { return overlay; }
private:
	BYTE* overlayBuffer;
	DWORD overlaySize;
	BYTE* overlay;
	BYTE* imageBuffer;
	DWORD imageSize;
	BYTE* image;

	DWORD width;
	DWORD height;
	bool display;
};

#endif	/* OVERLAY_H */

