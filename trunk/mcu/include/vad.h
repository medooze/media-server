/* 
 * File:   vad.h
 * Author: Sergio
 *
 * Created on 13 de agosto de 2012, 10:10
 */

#ifndef VAD_H
#define	VAD_H
#include "config.h"

class VADProxy
{
public:
	virtual DWORD GetVAD(int id) = 0;
};

#ifdef VADWEBRTC
extern "C" {
#include <common_audio/vad/vad_core.h>
}

class VAD
{
public:
	typedef enum { QUALITY=0,LOWBITRATE=1,AGGRESSIVE=2,VERYAGGRESIVE=3} Mode;
public:
	VAD();
	
	bool SetMode(Mode mode);
	int CalcVad8khz(SWORD* frame,DWORD size);
	int GetVAD();
private:
	VadInstT inst;
};
#else
class VAD
{
public:
	VAD(){};
	int CalcVad8khz(SWORD* frame,DWORD size) { return 0; }
	int GetVAD()				 { return 0; }
};
#endif
#endif	/* VAD_H */

