/* 
 * File:   audiodecoder.h
 * Author: Sergio
 *
 * Created on 1 de agosto de 2012, 13:34
 */

#ifndef AUDIODECODER_H
#define	AUDIODECODER_H
#include "codecs.h"
#include "audio.h"
#include "waitqueue.h"
#include "rtp.h"

class AudioDecoderWorker 
	: public RTPIncomingMediaStream::Listener
{
public:
	AudioDecoderWorker() = default;
	virtual ~AudioDecoderWorker();

	int Start();
	virtual void onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet);
	virtual void onEnded(RTPIncomingMediaStream* stream);
	virtual void onBye(RTPIncomingMediaStream* stream);
	int Stop();
	
	void SetAACConfig(const uint8_t* data,const size_t size);
	void AddAudioOuput(AudioOutput* ouput);
	void RemoveAudioOutput(AudioOutput* ouput);

protected:
	int Decode();

private:
	static void *startDecoding(void *par);

private:
	std::set<AudioOutput*> outputs;
	WaitQueue<RTPPacket::shared> packets;
	pthread_t thread;
	Mutex mutex;
	bool		decoding	= false;
	DWORD		rate		= 0;
	DWORD		numChannels = 0;
	std::unique_ptr<AudioDecoder>	codec;
};

#endif	/* AUDIODECODER_H */

