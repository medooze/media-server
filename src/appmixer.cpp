/*
 * File:   appmixer.cpp
 * Author: Sergio
 *
 * Created on 15 de enero de 2013, 15:38
 */

#include <netdb.h>

#include "appmixer.h"
#include "log.h"
#include "fifo.h"
#include "use.h"
#include "overlay.h"
#ifdef CEF
#include "cef/Browser.h"
#include "cef/keyboard.h"
#endif


AppMixer::AppMixer()
{
	//No output
	output = NULL;
	//No img
	img = NULL;
	
}
AppMixer::~AppMixer()
{
	//End just in case
	End();
}

int AppMixer::Init(VideoOutput* output)
{
	//Set output
	this->output = output;

	return true;
}

int AppMixer::DisplayImage(const char* filename)
{
	Log("-DisplayImage [\"%s\"]\n",filename);

	//Load image
	if (!logo.Load(filename))
		//Error
		return Error("-Error loading file");

	//Check output
	if (!output)
		//Error
		return Error("-No output");
	
	//Check max size
	if (logo.GetWidth()*logo.GetHeight()>4096*3072)
	{
		//CLose
		logo.Close();
		//Error
		return Error("-Size bigger than max size allowed (4096*3072)\n");
	}

	//Set size
	output->SetVideoSize(logo.GetWidth(),logo.GetHeight());
	//Set image
	output->NextFrame(logo.GetFrame());

	//Set size 
	SetSize(logo.GetWidth(),logo.GetHeight());

	
	//CLose
	logo.Close();

	//Everything ok
	return true;
}

int AppMixer::End()
{
	Log(">AppMixer::End\n");

	//Reset 
	Reset();
	
	//Reset output
	output = NULL;

	Log("<AppMixer::End\n");

}

int AppMixer::Reset()
{
	Log("-AppMixer::Reset\n");
	
	//If already got memory
	if (img)
	{
		//Free it
		free(img);
		//Null it
		img = NULL;
	}
}


int AppMixer::SetSize(int width,int height)
{
	Log("-AppMixer::SetSize [%dx%d]\n",width,height);

	//REset us
	Reset();

	//If got size
	if (width && height)
	{
		//Create number of pixels
		DWORD num = width*height;
		//Get size with padding
		DWORD size = (((width/32+1)*32)*((height/32+1)*32)*3)/2+AV_INPUT_BUFFER_PADDING_SIZE+32;
		//Allocate memory
		img = (BYTE*) malloc32(size);

		// paint the background in black for YUV
		memset(img	, 0		, num);
		memset(img+num	, (BYTE) -128	, num/2);

		//Set size
		if (output) output->SetVideoSize(width,height);
	}


	return 1;
}
