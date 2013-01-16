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
#include "video.h"
#include "logo.h"

class AppMixer
{
public:
	AppMixer();

	int Init(VideoOutput *output);
	int DisplayImage(const char* filename);
	int End();

private:
	Logo		logo;
	VideoOutput*	output;
};

#endif	/* APPMIXER_H */

