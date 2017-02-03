/* 
 * File:   appmixer.h
 * Author: Sergio
 *
 * Created on 15 de enero de 2013, 15:38
 */

#ifndef APPMIXER_H
#define	APPMIXER_H
#include <string>
#include "config.h"
#include "tools.h"
#include "video.h"
#include "logo.h"
#include "fifo.h"
#include "use.h"
#include "wait.h"
#include "overlay.h"


class AppMixer 
{
public:
	AppMixer();
	~AppMixer();

	int Init(VideoOutput *output);
	int DisplayImage(const char* filename);
	int Reset();
	int SetSize(int width,int height);
	int End();

private:
	int Display(const BYTE* frame,int x,int y,int width,int height,int lineSize);
private:
	Logo		logo;
	VideoOutput*	output;
	Use		use;
	BYTE*		img;
};	
#endif	/* APPMIXER_H */

