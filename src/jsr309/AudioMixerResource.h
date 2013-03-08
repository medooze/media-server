/* 
 * File:   AudioMixerResource.h
 * Author: Sergio
 *
 * Created on 13 de septiembre de 2011, 1:07
 */

#ifndef AUDIOMIXERRESOURCE_H
#define	AUDIOMIXERRESOURCE_H

#include "config.h"
#include "audiomixer.h"
#include "Joinable.h"
#include "AudioEncoderWorker.h"
#include "AudioDecoderWorker.h"
#include <string>

class AudioMixerResource
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
		AudioEncoderMultiplexerWorker encoder;
		AudioDecoderJoinableWorker decoder;
	};

public:
	AudioMixerResource(std::wstring &name);
	virtual ~AudioMixerResource();

	int Init();
	int CreatePort(std::wstring &tag);
	int SetPortCodec(int portId,AudioCodec::Type codec);
	int DeletePort(int portId);
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
	AudioMixer mixer;
	Ports ports;
	int maxId;
	bool inited;
	
};

#endif	/* AUDIOMIXERRESOURCE_H */

