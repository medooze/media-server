/* 
 * File:   opusdecoder.h
 * Author: Sergio
 *
 * Created on 14 de marzo de 2013, 11:19
 */

#ifndef OPUSDECODER_H
#define	OPUSDECODER_H
#include <opus/opus.h>
#include "config.h"
#include "audio.h"

class OpusDecoder : public AudioDecoder
{
public:
	OpusDecoder();
	virtual ~OpusDecoder();
	virtual int Decode(BYTE *in,int inLen,SWORD* out,int outLen);
private:
	OpusDecoder *dec;
};

#endif	/* OPUSDECODER_H */

