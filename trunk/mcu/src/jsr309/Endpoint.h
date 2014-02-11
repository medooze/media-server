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
	
	//Endpoint  functionality
	int StartSending(MediaFrame::Type media,char *sendIp,int sendPort,RTPMap& rtpMap);
	int StopSending(MediaFrame::Type media);
	int StartReceiving(MediaFrame::Type media,RTPMap& rtpMap);
	int StopReceiving(MediaFrame::Type media);
	int RequestUpdate(MediaFrame::Type media);

	int SetLocalCryptoSDES(MediaFrame::Type media,const char* suite, const char* key64);
	int SetRemoteCryptoSDES(MediaFrame::Type media,const char* suite, const char* key64);
	int SetRemoteCryptoDTLS(MediaFrame::Type media,const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(MediaFrame::Type media,const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(MediaFrame::Type media,const char* username, const char* pwd);
	int SetRTPProperties(MediaFrame::Type media,const Properties& properties);

	//Attach
	int Attach(MediaFrame::Type media, Joinable *join);
	int Dettach(MediaFrame::Type media);
	Joinable* GetJoinable(MediaFrame::Type media);

	std::wstring& GetName() { return name; }
private:
	RTPEndpoint* GetRTPEndpoint(MediaFrame::Type media)
	{
		switch(media)
		{
			case MediaFrame::Audio:
				return audio;
			case MediaFrame::Video:
				return video;
			case MediaFrame::Text:
				return text;
		}

		return NULL;
	}
private:
	std::wstring name;
	//RTP sessions
	RTPEndpoint *audio;
	RTPEndpoint *video;
	RTPEndpoint *text;
	RemoteRateEstimator estimator;
};

#endif	/* ENDPOINT_H */

