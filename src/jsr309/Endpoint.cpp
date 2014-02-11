/* 
 * File:   Endpoint.cpp
 * Author: Sergio
 * 
 * Created on 7 de septiembre de 2011, 0:59
 */
#include "log.h"
#include "Endpoint.h"

Endpoint::Endpoint(std::wstring name,bool audioSupported,bool videoSupported,bool textSupported) : estimator(name)
{
	//Store name
	this->name = name;
	//Nullify
	audio = NULL;
	video = NULL;
	text = NULL;
	//If audio
	if (audioSupported)
		//Create endpoint
		audio = new RTPEndpoint(MediaFrame::Audio);
	//If video
	if (videoSupported)
		//Create endpoint
		video = new RTPEndpoint(MediaFrame::Video);
	//If video
	if (textSupported)
		//Create endpoint
		text = new RTPEndpoint(MediaFrame::Text);
}

Endpoint::~Endpoint()
{
	//If endpoint for audio
	if (audio)
		//Delete endpoint
		delete(audio);
	//If endpoint for video
	if (video)
		//Delete endpoint
		delete(video);
	//If endpoint for audio
	if (text)
		//Delete endpoint
		delete(text);

}

//Methods
int Endpoint::Init()
{
	//If endpoint for audio
	if (audio)
		//Init it
		audio->Init();
	//If endpoint for video
	if (video)
	{
		//Init it
		video->Init();
		//Set estimator
		video->SetRemoteRateEstimator(&estimator);
	}
	//If endpoint for audio
	if (text)
		//Init it
		text->Init();
}

int Endpoint::End()
{
	Log(">End endpoint\n");

	//If endpoint for audio
	if (audio)
		//Init it
		audio->End();
	//If endpoint for video
	if (video)
		//Init it
		video->End();
	//If endpoint for audio
	if (text)
		//Init it
		text->End();

	Log("<End endpoint\n");
}

int Endpoint::StartSending(MediaFrame::Type media,char *sendIp,int sendPort,RTPMap& rtpMap)
{
	//Get enpoint
	RTPEndpoint* rtp = GetRTPEndpoint(media);
	
	//Check 
	if (!rtp)
		//Init it
		return Error("No media supported");

	//Stop sending for a while
	rtp->StopSending();

	//Set remote endpoint
	if(!rtp->SetRemotePort(sendIp,sendPort))
		//Error
		return Error("Error SetRemotePort\n");

	//Set sending map
	rtp->SetSendingRTPMap(rtpMap);

	//And send
	return rtp->StartSending();
}

int Endpoint::StopSending(MediaFrame::Type media)
{
	//Get rtp enpoint for media
	RTPEndpoint* rtp = GetRTPEndpoint(media);

	//Check
	if (!rtp)
		//Init it
		return Error("No media supported");
	
	//Stop sending for a while
	return rtp->StopSending();
}

int Endpoint::StartReceiving(MediaFrame::Type media,RTPMap& rtpMap)
{
	Log("-StartReceiving endpoint [name:%ls,media:%s]\n",name.c_str(),MediaFrame::TypeToString(media));
	
	//Get rtp enpoint for media
	RTPEndpoint* rtp = GetRTPEndpoint(media);

	//Check
	if (!rtp)
		//Init it
		return Error("No media supported");
	
	//Set map
	rtp->SetReceivingRTPMap(rtpMap);
	
	//Start
	if (!rtp->StartReceiving())
		//Exit
		return Error("Error starting receiving media");

	//Return port
	return rtp->GetLocalPort();
}

int Endpoint::StopReceiving(MediaFrame::Type media)
{
	//Get rtp enpoint for media
	RTPEndpoint* rtp = GetRTPEndpoint(media);

	//Check
	if (!rtp)
		//Init it
		return Error("No media supported");
	//Start
	return rtp->StopReceiving();
}

//Attach
int Endpoint::Attach(MediaFrame::Type media, Joinable *join)
{
	Log("-Endpoint attaching [media:%d]\n",media);
	
	//Depending on the type
	switch (media)
	{
		case MediaFrame::Audio:
			//Check if audio is supported
			if (!audio)
				//Error
				return Error("Audio not supported\n");
			//Attach audio
			return audio->Attach(join);
		case MediaFrame::Video:
			//Check if video is supported
			if (!video)
				//Error
				return Error("Video not supported\n");
			//Attach audio
			return video->Attach(join);
		case MediaFrame::Text:
			//Check if text is supported
			if (!text)
				//Error
				return Error("Text not supported\n");
			//Attach audio
			return text->Attach(join);
	}

	return 1;
}

int Endpoint::Dettach(MediaFrame::Type media)
{
	Log("-Endpoint detaching [media:%d]\n",media);

	//Depending on the type
	switch (media)
	{
		case MediaFrame::Audio:
			//Check if audio is supported
			if (!audio)
				//Error
				return Error("Audio not supported\n");
			//Attach audio
			return audio->Dettach();
		case MediaFrame::Video:
			//Check if video is supported
			if (!video)
				//Error
				return Error("Video not supported\n");
			//Attach audio
			return video->Dettach();
		case MediaFrame::Text:
			//Check if text is supported
			if (!text)
				//Error
				return Error("Text not supported\n");
			//Attach audio
			return text->Dettach();
	}

	return 1;
}
Joinable* Endpoint::GetJoinable(MediaFrame::Type media)
{
	//Depending on the type
	switch (media)
	{
		case MediaFrame::Audio:
			//Check
			if (audio)
				// audio
				return audio;
		case MediaFrame::Video:
			//Check
			if (video)
				//Attach audio
				return video;
		case MediaFrame::Text:
			//Check
			if (text)
				//text
				return text;
	}

	//Uhh?
	return NULL;
}


int Endpoint::RequestUpdate(MediaFrame::Type media)
{
	//Depending on the type
	switch (media)
	{
		case MediaFrame::Audio:
			//Check
			if (audio)
				// audio
				return audio->RequestUpdate();
		case MediaFrame::Video:
			//Check
			if (video)
				//Attach audio
				return video->RequestUpdate();
		case MediaFrame::Text:
			//Check
			if (text)
				//text
				return text->RequestUpdate();
		default:
			return Error("Unknown media [%d]\n",media);
	}

	//OK
	return 1;
}

int Endpoint::SetLocalCryptoSDES(MediaFrame::Type media,const char* suite, const char* key)
{
	switch (media)
	{
		case MediaFrame::Audio:
			if (audio)
				return audio->SetLocalCryptoSDES(suite,key);
		case MediaFrame::Video:
			if (video)
				return video->SetLocalCryptoSDES(suite,key);
		case MediaFrame::Text:
			if (text)
				return text->SetLocalCryptoSDES(suite,key);
		default:
			return Error("Unknown media [%d]\n",media);
	}

	//OK
	return 1;
}

int Endpoint::SetRemoteCryptoDTLS(MediaFrame::Type media,const char *setup,const char *hash,const char *fingerprint)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio->SetRemoteCryptoDTLS(setup,hash,fingerprint);
		case MediaFrame::Video:
			return video->SetRemoteCryptoDTLS(setup,hash,fingerprint);
		case MediaFrame::Text:
			return text->SetRemoteCryptoDTLS(setup,hash,fingerprint);
		default:
			return Error("Unknown media [%d]\n",media);
	}

	//OK
	return 1;
}

int Endpoint::SetRemoteCryptoSDES(MediaFrame::Type media,const char* suite, const char* key)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio->SetRemoteCryptoSDES(suite,key);
		case MediaFrame::Video:
			return video->SetRemoteCryptoSDES(suite,key);
		case MediaFrame::Text:
			return text->SetRemoteCryptoSDES(suite,key);
		default:
			return Error("Unknown media [%d]\n",media);
	}

	//OK
	return 1;
}

int Endpoint::SetLocalSTUNCredentials(MediaFrame::Type media,const char* username, const char* pwd)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio->SetLocalSTUNCredentials(username,pwd);
		case MediaFrame::Video:
			return video->SetLocalSTUNCredentials(username,pwd);
		case MediaFrame::Text:
			return text->SetLocalSTUNCredentials(username,pwd);
		default:
			return Error("Unknown media [%d]\n",media);
	}

	//OK
	return 1;
}

int Endpoint::SetRTPProperties(MediaFrame::Type media,const Properties& properties)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio->SetProperties(properties);
		case MediaFrame::Video:
			return video->SetProperties(properties);
		case MediaFrame::Text:
			return text->SetProperties(properties);
		default:
			return Error("Unknown media [%d]\n",media);
	}

	//OK
	return 1;
}

int Endpoint::SetRemoteSTUNCredentials(MediaFrame::Type media,const char* username, const char* pwd)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return audio->SetRemoteSTUNCredentials(username,pwd);
		case MediaFrame::Video:
			return video->SetRemoteSTUNCredentials(username,pwd);
		case MediaFrame::Text:
			return text->SetRemoteSTUNCredentials(username,pwd);
		default:
			return Error("Unknown media [%d]\n",media);
	}

	//OK
	return 1;
}