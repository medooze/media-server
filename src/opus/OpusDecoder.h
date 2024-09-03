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
	virtual int Decode(const std::shared_ptr<const AudioFrame>& frame);
	virtual AudioBuffer::shared GetDecodedAudioFrame();
	virtual DWORD TrySetRate(DWORD rate);
	virtual DWORD GetRate()	{ return rate;	}
private:
	OpusDecoder *dec;
	DWORD rate;
	int numChannels;
	std::pair<uint8_t*, int> audioFrameInfo;
};

#endif	/* OPUSDECODER_H */

