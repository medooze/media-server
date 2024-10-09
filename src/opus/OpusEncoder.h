/* 
 * File:   opusencoder.h
 * Author: Sergio
 *
 * Created on 14 de marzo de 2013, 11:19
 */

#ifndef OPUSENCODER_H
#define	OPUSENCODER_H
#include <opus/opus.h>
#include <opus/opusconfig.h>
#include "config.h"
#include "audio.h"

class OpusEncoder : public AudioEncoder
{
public:
	OpusEncoder(const Properties &properties);
	virtual ~OpusEncoder();
	virtual AudioFrame::shared Encode(const AudioBuffer::const_shared& audioBuffer);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels);
	virtual DWORD GetRate()			{ return rate;	}
	virtual DWORD GetNumChannels()		{ return numChannels; }
	virtual DWORD GetClockRate()		{ return 48000;	}
	virtual void SetConfig(DWORD rate, DWORD numChannels);
private:
	OpusEncoder *enc;
	AudioFrame::shared audioFrame;
	OpusConfig config = {};
	DWORD rate;
	DWORD numChannels;
	int mode;
};

#endif	/* OPUSENCODER_H */

