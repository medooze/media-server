/* 
 * File:   VideoTranscoder.h
 * Author: Sergio
 *
 * Created on 19 de marzo de 2013, 12:32
 */

#ifndef VIDEOTRANSCODER_H
#define	VIDEOTRANSCODER_H
#include "VideoEncoderWorkerMultiplexer.h"
#include "VideoDecoderWorker.h"
#include "videopipe.h"
#include <string>

class VideoTranscoder :
	public Joinable,
	public Joinable::Listener
{
public:
	VideoTranscoder(std::wstring &name);
	virtual ~VideoTranscoder();

	int Init();
	int SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod, const Properties & properties);
	int End();

	//Joinable interface
	virtual void AddListener(Joinable::Listener *listener);
	virtual void Update();
	virtual void SetREMB(DWORD estimation);
	virtual void RemoveListener(Joinable::Listener *listener);

	//Virtuals from Joinable::Listener
	virtual void onRTPPacket(RTPPacket &packet);
	virtual void onResetStream();
	virtual void onEndStream();

	//Attach
	int Attach(Joinable *join);
	int Dettach();

	const std::wstring& GetName() { return tag;	}

private:
	VideoEncoderMultiplexerWorker	encoder;
	VideoDecoderJoinableWorker	decoder;
	VideoPipe	pipe;
	std::wstring	tag;
	bool		inited;
};

#endif	/* VIDEOTRANSCODER_H */

