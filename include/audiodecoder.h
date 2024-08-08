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
	: public MediaFrame::Listener
{
public:
	AudioDecoderWorker() = default;
	virtual ~AudioDecoderWorker();

	int Start();
	int Stop();
	virtual void onMediaFrame(const MediaFrame& frame) override;
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame& frame)  override { onMediaFrame(frame); }

	void SetAACConfig(const uint8_t* data,const size_t size);
	void AddAudioOuput(AudioOutput* ouput);
	void RemoveAudioOutput(AudioOutput* ouput);

protected:
	int Decode();

private:
	static void *startDecoding(void *par);

private:
	std::set<AudioOutput*> outputs;
	WaitQueue<std::shared_ptr<AudioFrame>> frames;
	pthread_t thread = 0;
	Mutex mutex;
	bool		decoding	= false;
	DWORD		rate		= 0;
	DWORD		numChannels = 0;
	std::unique_ptr<AudioDecoder> audioDecoder;
};

#endif	/* AUDIODECODER_H */

