/* 
 * File:   Endpoint.cpp
 * Author: Sergio
 * 
 * Created on 7 de septiembre de 2011, 0:59
 */
#include "log.h"
#include "Endpoint.h"

Endpoint::Endpoint(std::wstring name,bool audioSupported,bool videoSupported,bool textSupported)
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
		//Init it
		video->Init();
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

//Endpoint Video functionality
int Endpoint::StartSendingVideo(char *sendVideoIp,int sendVideoPort,VideoCodec::RTPMap& rtpMap)
{
	//Check video
	if (!video)
		//Init it
		return Error("No video supported");

	//Stop sending for a while
	video->StopSending();

	//Set remote endpoint
	if(!video->SetRemotePort(sendVideoIp,sendVideoPort))
		//Error
		return Error("Error SetRemotePort\n");

	//Set sending map
	video->SetSendingVideoRTPMap(rtpMap);

	//And send
	return video->StartSending();
}

int Endpoint::StopSendingVideo()
{
	//Check video
	if (!video)
		//Init it
		return Error("No video supported");
	
	//Stop sending for a while
	return video->StopSending();
}

int Endpoint::StartReceivingVideo(VideoCodec::RTPMap& rtpMap)
{
	//Check video
	if (!video)
		//Init it
		return Error("No video supported");
	
	//Set map
	video->SetReceivingVideoRTPMap(rtpMap);
	
	//Start
	if (!video->StartReceiving())
		//Exit
		return Error("Error starting video receiving");

	//Return port
	return video->GetLocalPort();
}

int Endpoint::StopReceivingVideo()
{
	//Check video
	if (!video)
		//Init it
		return Error("No video supported");
	//Start
	return video->StopReceiving();
}

// Audio functionality
//Endpoint Audio functionality
int Endpoint::StartSendingAudio(char *sendAudioIp,int sendAudioPort,AudioCodec::RTPMap& rtpMap)
{
	//Check audio
	if (!audio)
		//Init it
		return Error("No audio supported");

	//Stop sending for a while
	audio->StopSending();

	//Set remote endpoint
	if(!audio->SetRemotePort(sendAudioIp,sendAudioPort))
		//Error
		return Error("Error SetRemotePort\n");

	//Set sending map
	audio->SetSendingAudioRTPMap(rtpMap);

	//And send
	return audio->StartSending();
}

int Endpoint::StopSendingAudio()
{
	//Check audio
	if (!audio)
		//Init it
		return Error("No audio supported");

	//Stop sending for a while
	return audio->StopSending();
}

int Endpoint::StartReceivingAudio(AudioCodec::RTPMap& rtpMap)
{
	//Check audio
	if (!audio)
		//Init it
		return Error("No audio supported");

	//Set map
	audio->SetReceivingAudioRTPMap(rtpMap);

	//Start
	if (!audio->StartReceiving())
		//Exit
		return Error("Error starting audio receiving");

	//Return port
	return audio->GetLocalPort();
}

int Endpoint::StopReceivingAudio()
{
	//Check audio
	if (!audio)
		//Init it
		return Error("No audio supported");
	//Start
	return audio->StopReceiving();
}

//Endpoint Text functionality
int Endpoint::StartSendingText(char *sendTextIp,int sendTextPort,TextCodec::RTPMap& rtpMap)
{
	//Check text
	if (!text)
		//Init it
		return Error("No text supported");

	//Stop sending for a while
	text->StopSending();

	//Set remote endpoint
	if(!text->SetRemotePort(sendTextIp,sendTextPort))
		//Error
		return Error("Error SetRemotePort\n");

	//Set sending map
	text->SetSendingTextRTPMap(rtpMap);

	//And send
	return text->StartSending();
}

int Endpoint::StopSendingText()
{
	//Check text
	if (!text)
		//Init it
		return Error("No text supported");

	//Stop sending for a while
	return text->StopSending();
}

int Endpoint::StartReceivingText(TextCodec::RTPMap& rtpMap)
{
	//Check text
	if (!text)
		//Init it
		return Error("No text supported");

	//Set map
	text->SetReceivingTextRTPMap(rtpMap);

	//Start
	if (!text->StartReceiving())
		//Exit
		return Error("Error starting audio receiving");

	//Return port
	return text->GetLocalPort();
}

int Endpoint::StopReceivingText()
{
	//Check text
	if (!text)
		//Init it
		return Error("No text supported");
	//Start
	return text->StopReceiving();
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
	}

	//Nothing
	return 0;
}
