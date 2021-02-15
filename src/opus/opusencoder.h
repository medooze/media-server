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
	OpusEncoder(const Properties &properties);
	virtual ~OpusEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels);
	virtual DWORD GetRate()			{ return rate;	}
	virtual DWORD GetNumChannels()		{ return numChannels; }
	virtual DWORD GetClockRate()		{ return 48000;	}
private:
	OpusEncoder *enc;
	DWORD rate;
	DWORD numChannels;
	int mode;
};

#endif	/* OPUSENCODER_H */

