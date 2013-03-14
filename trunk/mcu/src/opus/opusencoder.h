/* 
 * File:   opusencoder.h
 * Author: Sergio
 *
 * Created on 14 de marzo de 2013, 11:19
 */

#ifndef OPUSENCODER_H
#define	OPUSENCODER_H
#include <opus/opus.h>
#include "config.h"
#include "audio.h"

class OpusEncoder : public AudioEncoder
{
public:
	OpusEncoder();
	virtual ~OpusEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
private:
	OpusEncoder *enc;
};

#endif	/* OPUSENCODER_H */

