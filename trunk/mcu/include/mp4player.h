#ifndef _MP4PLAYER_H_
#define _MP4PLAYER_H_

#include "pipeaudioinput.h"
#include "pipevideoinput.h"
#include "pipetextinput.h"
#include "mp4streamer.h"
#include "codecs.h"



class MP4Player : public MP4Streamer::Listener
{
public:
	MP4Player();
	~MP4Player();

	int Init(AudioOutput *audioOutput,VideoOutput *videoOutput,TextOutput *textOutput);
	int Play(const char* filename, bool loop);
	int Stop();
	int End();

	virtual void onRTPPacket(RTPPacket &packet);
	virtual void onTextFrame(TextFrame &text);
	virtual void onMediaFrame(MediaFrame &frame);
	virtual void onMediaFrame(DWORD ssrc, MediaFrame &frame);
	virtual void onEnd();

private:
	
	MP4Streamer streamer;
	AudioDecoder *audioDecoder;
	VideoDecoder *videoDecoder;
	VideoOutput *videoOutput;
	AudioOutput *audioOutput;
	TextOutput  *textOutput;
	bool loop;
};
#endif
