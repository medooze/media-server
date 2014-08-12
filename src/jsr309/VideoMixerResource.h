/* 
 * File:   VideoMixerResource.h
 * Author: Sergio
 *
 * Created on 2 de noviembre de 2011, 23:38
 */

#ifndef VIDEOMIXERRESOURCE_H
#define	VIDEOMIXERRESOURCE_H

#include "config.h"
#include "mosaic.h"
#include "videomixer.h"
#include "Joinable.h"
#include "VideoEncoderWorkerMultiplexer.h"
#include "VideoDecoderWorker.h"
#include <string>

class VideoMixerResource
{
public:
	class Port
	{
	public:
		Port(std::wstring &tag)
		{
			this->tag = tag;
		}
		std::wstring	tag;
		VideoEncoderMultiplexerWorker encoder;
		VideoDecoderJoinableWorker decoder;
	};

public:
	VideoMixerResource(std::wstring &name);
	virtual ~VideoMixerResource();

	int Init(Mosaic::Type comp,int size);
	int CreatePort(std::wstring &tag,int mosaicId);
	int SetPortCodec(int portId,VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties &properties);
	int DeletePort(int portId);
	int CreateMosaic(Mosaic::Type comp,int size);
	int AddMosaicParticipant(int mosaicId,int portId);
	int RemoveMosaicParticipant(int mosaicId,int portId);
	int SetSlot(int mosaicId,int num,int id);
	int SetCompositionType(int mosaicId,Mosaic::Type comp,int size);
	int SetOverlayPNG(int mosaicId,const char* overlay);
	int ResetOverlay(int mosaicId);
	int DeleteMosaic(int mosaicId);
	int End();
	//Get joinables
	Joinable *GetJoinable(int portId);
	//Port Attach  to
	int Attach(int portId,Joinable *);
	int Dettach(int portId);

private:
	typedef std::map<int,Port*> Ports;

private:
	std::wstring tag;
	VideoMixer mixer;
	Ports ports;
	int maxId;
	bool inited;
};

#endif	/* VIDEOMIXERRESOURCE_H */

