/* 
 * File:   Endpoint.h
 * Author: Sergio
 *
 * Created on 7 de septiembre de 2011, 0:59
 */

#ifndef ENDPOINT_H
#define	ENDPOINT_H
#include "Joinable.h"
#include "rtpsession.h"
#include "RTPEndpoint.h"

class Endpoint
{
public:
	Endpoint(std::wstring name,bool audioSupported,bool videoSupported,bool textSupport);
	~Endpoint();
	
	//Methods
	int Init();
	int End();
	
	//Endpoint Video functionality
	int StartSendingVideo(char *sendVideoIp,int sendVideoPort,VideoCodec::RTPMap& rtpMap);
	int StopSendingVideo();
	int StartReceivingVideo(VideoCodec::RTPMap& rtpMap);
	int StopReceivingVideo();

	// Audio functionality
	int StartSendingAudio(char *sendAudioIp,int sendAudioPort,AudioCodec::RTPMap& rtpMap);
	int StopSendingAudio();
	int StartReceivingAudio(AudioCodec::RTPMap& rtpMap);
	int StopReceivingAudio();

	// Text functionality
	int StartSendingText(char *sendTextIp,int sendTextPort,TextCodec::RTPMap& rtpMap);
	int StopSendingText();
	int StartReceivingText(TextCodec::RTPMap& rtpMap);
	int StopReceivingText();

	int RequestUpdate(MediaFrame::Type media);

	//Attach
	int Attach(MediaFrame::Type media, Joinable *join);
	int Dettach(MediaFrame::Type media);
	Joinable* GetJoinable(MediaFrame::Type media);

	std::wstring& GetName() { return name; }
	
private:
	std::wstring name;
	//RTP sessions
	RTPEndpoint *audio;
	RTPEndpoint *video;
	RTPEndpoint *text;
};

#endif	/* ENDPOINT_H */

