/* 
 * File:   rtmpparticipant.cpp
 * Author: Sergio
 * 
 * Created on 19 de enero de 2012, 18:43
 */
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include "log.h"
#include "rtmpparticipant.h"
#include "codecs.h"
#include "rtpsession.h"
#include "video.h"
#include "h263/h263codec.h"
#include "flv1/flv1codec.h"
#include "avcdescriptor.h"
#include "fifo.h"
#include <wchar.h>
#include <stdlib.h>

RTMPParticipant::RTMPParticipant(DWORD partId) :
	Participant(Participant::RTMP,partId),
	RTMPMediaStream(0)
{
	//Neither sending nor receiving
	sendingAudio = false;
	sendingVideo = false;
	sendingText = false;
	receivingAudio = false;
	receivingVideo = false;
	sendFPU = false;
	//set mutes
	audioMuted = false;
	videoMuted = false;
	textMuted = false;
	meta = NULL;
	inited = false;
	//NOt attached
	attached = NULL;
	//Inicializamos los mutex
	pthread_mutex_init(&mutex,NULL);
}

RTMPParticipant::~RTMPParticipant()
{
	//End it just in case
	End();
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

int RTMPParticipant::SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int quality,int fillLevel,int intraPeriod)
{
	//LO guardamos
	videoCodec=codec;

	//Guardamos el bitrate
	videoBitrate=bitrate;

	//Store intra period
	videoIntraPeriod = intraPeriod;

	//Get width and height
	videoWidth = GetWidth(mode);
	videoHeight = GetHeight(mode);

	//Check size
	if (!videoWidth || !videoHeight)
		//Error
		return Error("Unknown video mode\n");

	//Y los fps
	videoFPS=fps;
}

int RTMPParticipant::SetAudioCodec(AudioCodec::Type codec)
{
	//Set it
	audioCodec = codec;
}

int RTMPParticipant::SetTextCodec(TextCodec::Type codec)
{
	//do nothing
	return true;
}

int RTMPParticipant::SendVideoFPU()
{
	//Flag it
	sendFPU = true;
	//Send it
	return true;
}

int RTMPParticipant::Init()
{
	//We are started
	inited = true;
	//Wait for first Iframe
	waitVideo = true;
	//Set first timestamp
	getUpdDifTime(&first);
	//Ok
	return true;
}

bool RTMPParticipant::StartSending()
{
	Log(">RTMPParticipant start sending\n");
	
	//Check it
	if (!inited)
		//Exit
		return Error("Not inited\n");

	//Start all
	StartSendingAudio();
	StartSendingVideo();
	StartSendingText();

	Log("<RTMPParticipant start sending\n");

	//OK
	return true;
}

void RTMPParticipant::StopSending()
{
	Log(">RTMPParticipant stop sending\n");

	//Stop everithing
	StopSendingAudio();
	StopSendingVideo();
	StopSendingText();

	Log("<RTMPParticipant stop sending\n");
}

bool RTMPParticipant::StartReceiving()
{
	Log(">RTMPParticipant start receiving\n");

	//Check it
	if (!inited)
		//Exit
		return Error("-StartReceiving without beeing inited\n");
	
	//Start all
	StartReceivingAudio();
	StartReceivingVideo();

	Log("<RTMPParticipant start receiving\n");

	//OK
	return true;
}

void RTMPParticipant::StopReceiving()
{
	Log(">RTMPParticipant stop receiving\n");

	//Stop everithing
	StopReceivingAudio();
	StopReceivingVideo();

	Log("<RTMPParticipant stop receiving\n");
}

int RTMPParticipant::End()
{
	//Check if we are running
	if (!inited)
		return false;

	//Stop sending
	StopSending();
	//And receiver
	StopReceiving();

	//Send stream end
	SendStreamEnd();

	//Check
	if (attached)
		//Remove from that listeners
		attached->RemoveMediaListener(this);
	//Not attached anymore
	attached = NULL;

	//Remove meta
	if (meta)
		//delete objet
		delete(meta);
	//No meta
	meta = NULL;

	//Stop
	inited=0;

	return true;
}
int  RTMPParticipant::StartSendingVideo()
{
	Log("-StartSendingVideo\n");

	//Si estabamos mandando tenemos que parar
	if (sendingVideo)
		//Y esperamos que salga
		StopSendingVideo();

	//Estamos mandando
	sendingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&sendVideoThread,startSendingVideo,this,0);

	return sendingVideo;
}

int  RTMPParticipant::StopSendingVideo()
{
	Log(">StopSendingVideo\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (sendingVideo)
	{
		//Dejamos de recivir
		sendingVideo=0;

		//Cancel frame cpature
		videoInput->CancelGrabFrame();

		//Esperamos
		pthread_join(sendVideoThread,NULL);
	}

	Log("<StopSendingVideo\n");
}

int  RTMPParticipant::StartReceivingVideo()
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingVideo)
		//Stop receiving
		StopReceivingVideo();

	//Reset video frames queue
	videoFrames.Reset();

	//Estamos recibiendo
	receivingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&recVideoThread,startReceivingVideo,this,0);

	//Logeamos
	Log("-StartReceivingVideo\n");

	return 1;
}

int  RTMPParticipant::StopReceivingVideo()
{
	Log(">StopReceivingVideo\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingVideo)
	{
		//Dejamos de recivir
		receivingVideo=0;

		//Cancel any pending wait
		videoFrames.Cancel();

		//Esperamos
		pthread_join(recVideoThread,NULL);
	}

	Log("<StopReceivingVideo\n");

	return 1;
}

int  RTMPParticipant::StartSendingAudio()
{
	Log("-StartSendingAudio\n");

	//Si estabamos mandando tenemos que parar
	if (sendingAudio)
		//Y esperamos que salga
		StopSendingAudio();

	//Estamos mandando
	sendingAudio=1;

	//Arrancamos los procesos
	createPriorityThread(&sendAudioThread,startSendingAudio,this,0);

	return sendingAudio;
}

int  RTMPParticipant::StopSendingAudio()
{
	Log(">StopSendingAudio\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (sendingAudio)
	{
		//Dejamos de recivir
		sendingAudio=0;

		//Cancel grab audio
		audioInput->CancelRecBuffer();
		
		//Esperamos
		pthread_join(sendAudioThread,NULL);
	}

	Log("<StopSendingAudio\n");

	return sendingAudio;
}

int  RTMPParticipant::StartReceivingAudio()
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingAudio)
		//Stop receiving audio
		StopReceivingAudio();

	//Estamos recibiendo
	receivingAudio=1;

	//Reset audio frames queue
	audioFrames.Reset();

	//Arrancamos los procesos
	createPriorityThread(&recAudioThread,startReceivingAudio,this,0);

	//Logeamos
	Log("-StartReceivingAudio\n");

	return receivingAudio;
}

int  RTMPParticipant::StopReceivingAudio()
{
	Log(">StopReceivingAudio\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingAudio)
	{
		//Dejamos de recivir
		receivingAudio=0;

		//Cancel any pending wait
		audioFrames.Cancel();

		//Esperamos
		pthread_join(recAudioThread,NULL);
	}

	Log("<StopReceivingAudio\n");

	return 1;
}

void* RTMPParticipant::startSendingText(void *par)
{
	Log("RecTextThread [%d]\n",getpid());

	//Obtenemos el objeto
	RTMPParticipant *sess = (RTMPParticipant *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->SendText());
}

int  RTMPParticipant::StartSendingText()
{
	Log("-StartSendingText\n");

	//Si estabamos mandando tenemos que parar
	if (sendingText)
		//Y esperamos que salga
		StopSendingText();

	
	//Estamos mandando
	sendingText=1;

	return sendingText;
}

int  RTMPParticipant::StopSendingText()
{
	Log(">StopSendingText\n");

	//Dejamos de recivir
	sendingText=0;

	Log("<StopSendingText\n");

	return 1;
}

/**************************************
* startReceivingVideo
*	Function helper for thread
**************************************/
void* RTMPParticipant::startReceivingVideo(void *par)
{
	Log("RecVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	RTMPParticipant *sess = (RTMPParticipant *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->RecVideo());
}

/**************************************
* startSendingVideo
*	Function helper for thread
**************************************/
void* RTMPParticipant::startSendingVideo(void *par)
{
	Log("SendVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	RTMPParticipant *sess = (RTMPParticipant *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->SendVideo());
}

/**************************************
* startReceivingAudio
*	Function helper for thread
**************************************/
void* RTMPParticipant::startReceivingAudio(void *par)
{
	Log("RecVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	RTMPParticipant *sess = (RTMPParticipant *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->RecAudio());
}


/**************************************
* startSendingAudio
*	Function helper for thread
**************************************/
void* RTMPParticipant::startSendingAudio(void *par)
{
	Log("SendAudioThread [%d]\n",getpid());

	//Obtenemos el objeto
	RTMPParticipant *sess = (RTMPParticipant *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->SendAudio());
}

int RTMPParticipant::SendVideo()
{
	timeval t;
	//Get now
	getUpdDifTime(&t);
	//Coders
	VideoEncoder* encoder = VideoCodecFactory::CreateEncoder(videoCodec);
	//Create new video frame
	RTMPVideoFrame  frame(0,65535);

	Log(">SendVideo\n");
	
	//Set codec
	switch(videoCodec)
	{
		case VideoCodec::SORENSON:
			//Set codec
			frame.SetVideoCodec(RTMPVideoFrame::FLV1);
			break;
		case VideoCodec::H264:
			//Set codec
			frame.SetVideoCodec(RTMPVideoFrame::AVC);
			//Set NALU type
			frame.SetAVCType(RTMPVideoFrame::AVCNALU);
			//Set no delay
			frame.SetAVCTS(0);
			break;
		default:
			return Error("Codec not supported\n");
	}

	//Set sice
	videoInput->StartVideoCapture(videoWidth,videoHeight,videoFPS);
	//Set bitrate
	encoder->SetFrameRate(videoFPS,videoBitrate,videoIntraPeriod);
	//Set size
	encoder->SetSize(videoWidth,videoHeight);

	//Mientras tengamos que capturar
	while(sendingVideo)
	{
		//Nos quedamos con el puntero antes de que lo cambien
		BYTE *pic = videoInput->GrabFrame();
		//Check picture
		if (!pic)
		{
			Log("-no pic\n");
			//Exit
			continue;
		}

		//Check if we need to send intra
		if (sendFPU)
		{
			//Set it
			encoder->FastPictureUpdate();
			//Do not send anymore
			sendFPU = false;
		}
		
		//Encode next frame
		//Log(">Encoding frame [%lld]\n",getUpdDifTime(&t));
		VideoFrame *encoded = encoder->EncodeFrame(pic,videoInput->GetBufferSize());
		//Log("<Encoding frame [%lld]\n",getUpdDifTime(&t));
		
		//Check
		if (!encoded)
			break;

		//Check size
		if (frame.GetMaxMediaSize()<encoded->GetLength())
			//Not enougth space
			return Error("Not enought space to copy FLV encodec frame [frame:%d,encoded:%d",frame.GetMaxMediaSize(),encoded->GetLength());

		//Get full frame
		frame.SetVideoFrame(encoded->GetData(),encoded->GetLength());

		//Set buffer size
		frame.SetMediaSize(encoded->GetLength());

		//Set timestamp
		frame.SetTimestamp(getDifTime(&first)/1000);
		
		//Check type
		if (encoded->IsIntra())
		{
			//Log("-is intra frame\n");
			//Set type
			frame.SetFrameType(RTMPVideoFrame::INTRA);
			//Check if it is an AVC and not send the description
			if (frame.GetVideoCodec()==RTMPVideoFrame::AVC)
			{
				//Create description
				AVCDescriptor desc;
				//Set values
				desc.SetConfigurationVersion(1);
				desc.SetAVCProfileIndication(0x42);
				desc.SetProfileCompatibility(0x80);
				desc.SetAVCLevelIndication(0x0C);
				desc.SetNALUnitLength(3);
				//Get encoded data
				BYTE *data = encoded->GetData();
				//Get size
				DWORD size = encoded->GetLength();
				//Chop into NALs
				while(size>4)
				{
					//Get nal size
					DWORD nalSize =  get4(data,0);
					//Get NAL start
					BYTE *nal = data+4;
					//Depending on the type
					switch(nal[0] & 0xF)
					{
						case 8:
							//Append
							desc.AddPictureParameterSet(nal,nalSize);
							break;
						case 7:
							//Append
							desc.AddSequenceParameterSet(nal,nalSize);
							break;
					}
					//Skip it
					data+=4+nalSize;
					size-=4+nalSize;
				}

				//Crete desc frame
				RTMPVideoFrame frameDesc(frame.GetTimestamp(),desc);
				//Dump it
				frameDesc.Dump();
				//Send it
				SendMediaFrame(&frameDesc);
			}
		} else {
			//Set type
			frame.SetFrameType(RTMPVideoFrame::INTER);
		}
		//Send it
		SendMediaFrame(&frame);
	}

	//Stop capture
	videoInput->StopVideoCapture();
	
	//Check
	if (encoder)
		//Delete
		delete(encoder);

	Log("<SendVideo\n");

	//Salimos
	pthread_exit(0);
}

int RTMPParticipant::SendAudio()
{
	AudioCodec*	rtmpAudioEncoder = NULL;

	//Create new audio frame
	RTMPAudioFrame  *audio = new RTMPAudioFrame(0,MTU);
	
        struct timeval 	before;

	Log(">SendAudio\n");

	//Obtenemos el tiempo ahora
	gettimeofday(&before,NULL);

	//Creamos el codec de audio
	if ((rtmpAudioEncoder = AudioCodec::CreateCodec(AudioCodec::NELLY11))==NULL)
	{
		Log("Error en el envio de audio,saliendo\n");
		return 0;
	}

	//Empezamos a grabar
	audioInput->StartRecording();

	//Mientras tengamos que capturar
	while(sendingAudio)
	{
		SWORD recBuffer[512];

		//Capturamos
		DWORD  recLen = audioInput->RecBuffer(recBuffer,rtmpAudioEncoder->numFrameSamples);

		//Check len
		if (!recLen)
		{
			Log("-sendingAudio cont\n");
			continue;
		}

		//Rencode it
		DWORD len;

		while((len=rtmpAudioEncoder->Encode(recBuffer,recLen,audio->GetMediaData(),audio->GetMaxMediaSize()))>0)
		{
			//REset
			recLen = 0;

			//Set length
			audio->SetMediaSize(len);

			switch(rtmpAudioEncoder->type)
			{
				case AudioCodec::SPEEX16:
					//Set RTMP data
					audio->SetAudioCodec(RTMPAudioFrame::SPEEX);
					audio->SetSoundRate(RTMPAudioFrame::RATE11khz);
					audio->SetSamples16Bits(1);
					audio->SetStereo(0);
					break;
				case AudioCodec::NELLY8:
					//Set RTMP data
					audio->SetAudioCodec(RTMPAudioFrame::NELLY8khz);
					audio->SetSoundRate(RTMPAudioFrame::RATE11khz);
					audio->SetSamples16Bits(1);
					audio->SetStereo(0);
					break;
				case AudioCodec::NELLY11:
					//Set RTMP data
					audio->SetAudioCodec(RTMPAudioFrame::NELLY);
					audio->SetSoundRate(RTMPAudioFrame::RATE11khz);
					audio->SetSamples16Bits(1);
					audio->SetStereo(0);
					break;
			}

			//Set timestamp
			audio->SetTimestamp(getDifTime(&first)/1000);

			//Send audio
			SendMediaFrame(audio);
		}
	}

	//Check
	if (audio)
		//Delete it
		delete(audio);

	Log("<SendAudio\n");

	//Salimos
	pthread_exit(0);
}

int RTMPParticipant::SendText()
{
	Log(">SendText\n");

	//Mientras tengamos que capturar
	while(sendingText)
	{
		//Text frame
		TextFrame *frame = NULL;

		//Get frame
		frame = textInput->GetFrame(0);

		//Create new timestamp associated to latest media time
		RTMPMetaData *meta = new RTMPMetaData(frame->GetTimeStamp());

		//Add text name
		meta->AddParam(new AMFString(L"onText"));
		//Set data
		meta->AddParam(new AMFString(frame->GetWChar()));

		//Debug
		Log("Got T140 frame\n");
		meta->Dump();

		//Check receiver
		SendMetaData(meta);

		//Delete frame
		delete(frame);
	}

	Log("<SendText\n");

	//Salimos
	pthread_exit(0);
}

int RTMPParticipant::RecVideo()
{
	VideoDecoder *decoder = NULL;
	DWORD width = 0;
	DWORD height = 0;
	BYTE NALUnitLength = 0;

	Log(">RecVideo\n");

	//While sending video
	while (receivingVideo)
	{
		//Wait for next video
		if (!videoFrames.Wait(0))
			//Check again
			continue;

		//Get audio grame
		RTMPVideoFrame* video = videoFrames.Pop();
		//check
		if (!video)
			//Again
			continue;

		//Check type to find decoder
		switch(video->GetVideoCodec())
		{
			case RTMPVideoFrame::FLV1:
				//Check if there is no decoder of of different type
				if (!decoder || decoder->type!=VideoCodec::SORENSON)
				{
					//check decoder
					if (decoder)
						//Delet
						delete(decoder);
					//Create sorenson one
					decoder = VideoCodecFactory::CreateDecoder(VideoCodec::SORENSON);
				}
				break;
			case RTMPVideoFrame::AVC:
				//Check if there is no decoder of of different type
				if (!decoder || decoder->type!=VideoCodec::H264)
				{
					//check decoder
					if (decoder)
						//Delet
						delete(decoder);
					//Create sorenson one
					decoder = VideoCodecFactory::CreateDecoder(VideoCodec::H264);
				}
				break;
			default:
				//Not found, ignore
				continue;
		}

		//Check if it is AVC descriptor
		if (video->GetVideoCodec()==RTMPVideoFrame::AVC && video->GetAVCType()==0)
		{
			AVCDescriptor desc;

			//Parse it
			if (!desc.Parse(video->GetMediaData(),video->GetMaxMediaSize()))
			{
				//Show error
				Error("AVCDescriptor parse error\n");
				//Dump it
				desc.Dump();
				//Skip
				continue;
			}

			//Get nal
			NALUnitLength = desc.GetNALUnitLength()+1;

			//Decode SPS
			for (int i=0;i<desc.GetNumOfSequenceParameterSets();i++)
				//Decode NAL
				decoder->DecodePacket(desc.GetSequenceParameterSet(i),desc.GetSequenceParameterSetSize(i),0,0);
			//Decode PPS
			for (int i=0;i<desc.GetNumOfPictureParameterSets();i++)
				//Decode NAL
				decoder->DecodePacket(desc.GetPictureParameterSet(i),desc.GetPictureParameterSetSize(i),0,0);

			//Nothing more needded;
			continue;
		} else if (video->GetVideoCodec()==RTMPVideoFrame::AVC && video->GetAVCType()==1) {
			//Malloc
			BYTE *data = video->GetMediaData();
			//Get size
			DWORD size = video->GetMediaSize();
			//Chop into NALs
			while(size>NALUnitLength)
			{
				DWORD nalSize = 0;
				//Get size
				if (NALUnitLength==4)
					//Get size
					nalSize = get4(data,0);
				else if (NALUnitLength==3)
					//Get size
					nalSize = get3(data,0);
				else if (NALUnitLength==2)
					//Get size
					nalSize = get2(data,0);
				else if (NALUnitLength==1)
					//Get size
					nalSize = data[0];
				else
					//Skip
					continue;
				//Get NAL start
				BYTE *nal = data+NALUnitLength;
				//Skip it
				data+=NALUnitLength+nalSize;
				size-=NALUnitLength+nalSize;
				//Decode it
				decoder->DecodePacket(nal,nalSize,0,(size<NALUnitLength));
			}
		} else {
			//Decode frame
			if (!decoder->Decode(video->GetMediaData(),video->GetMediaSize()))
			{
				Error("decode packet error");
				//Next
				continue;
			}
		}

		//Check size
		if (decoder->GetWidth()!=width || decoder->GetHeight()!=height)
		{
			//Get dimension
			width = decoder->GetWidth();
			height = decoder->GetHeight();

			//Set them in the encoder
			videoOutput->SetVideoSize(width,height);
		}

		//If it is muted
		if (!videoMuted)
			//Send
			videoOutput->NextFrame(decoder->GetFrame());

		//Delete video frame
		delete(video);
	}

	Log("<RecVideo\n");

	return 1;
}

int RTMPParticipant::RecAudio()
{
	AudioCodec::Type rtmpAudioCodec;
	AudioCodec *rtmpAudioDecoder = NULL;
	VAD vad;
	
	Log(">RecAudio\n");

	//While sending audio
	while (receivingAudio)
	{
		//Wait for next audio
		if (!audioFrames.Wait(0))
			//Check again
			continue;

		//Get audio grame
		RTMPAudioFrame* audio = audioFrames.Pop();

		//Check audio frame
		if (!audio)
			//Next one
			continue;
		
		//Get codec type
		switch(audio->GetAudioCodec())
		{
			case RTMPAudioFrame::SPEEX:
				//Set codec
				rtmpAudioCodec = AudioCodec::SPEEX16;
				break;
			case RTMPAudioFrame::NELLY:
				//Set codec type
				rtmpAudioCodec = AudioCodec::NELLY11;
				break;
			default:
				continue;
		}

		SWORD raw[512];
		DWORD rawSize = 512;
		DWORD rawLen = 0;

		//Check if we have a decoder
		if (!rtmpAudioDecoder || rtmpAudioDecoder->type!=rtmpAudioCodec)
		{
			//Check
			if (rtmpAudioDecoder)
				//Delete old one
				delete(rtmpAudioDecoder);
			//Create new one
			rtmpAudioDecoder = AudioCodec::CreateDecoder(rtmpAudioCodec);
		}

		//Get data
		BYTE *data = audio->GetMediaData();
		//Get size
		DWORD size = audio->GetMediaSize();
		//Decode it until no frame is found
		while ((rawLen = rtmpAudioDecoder->Decode(data,size,raw,rawSize))>0)
		{
			//Check size
			if (rawLen>0 && !audioMuted)
			{
				//Enqeueue it
				audioOutput->PlayBuffer(raw,rawLen,0);
				//calculate vad
				vad.CalcVad8khz(raw,rawLen);
			}
			//Remove size
			size = 0;
		}
		
		//Delete audio
		delete(audio);
	}

	Log("<RecAudio\n");

	return 1;
}

MediaStatistics RTMPParticipant::GetStatistics(MediaFrame::Type type)
{
	//Depending on the type
	switch (type)
	{
		case MediaFrame::Audio:
			return audioStats;
		case MediaFrame::Video:
			return videoStats;
		default:
			return textStats;
	}
}

int RTMPParticipant::SetMute(MediaFrame::Type media, bool isMuted)
{
	//Depending on the type
	switch (media)
	{
		case MediaFrame::Audio:
			//Set mute
			audioMuted = isMuted;
			//Exit
			return 1;
		case MediaFrame::Video:
			//Set mute
			videoMuted = isMuted;
			//Exit
			return 1;
		case MediaFrame::Text:
			//Set mute
			textMuted = isMuted;
			//Exit
			return 1;
	}

	return 0;
}

DWORD RTMPParticipant::AddMediaListener(RTMPMediaStream::Listener *listener)
{
	//Check inited
	if (!inited)
		//Exit
		return Error("RTMP participant not inited when trying to add listener\n");

	//Check listener
	if (!listener)
		//Do not add
		return GetNumListeners();

	//Call parent
	DWORD num = RTMPMediaStream::AddMediaListener(listener);
	//Call parent
	if (num==1)
		//Start sending
		StartSending();

	//If we already have metadata
	if (meta)
		//Send it
		listener->onMetaData(RTMPMediaStream::id,meta);

	//Return number of listeners
	return num;
}


DWORD RTMPParticipant::RemoveMediaListener(RTMPMediaStream::Listener *listener)
{
	//Call parent
	DWORD num = RTMPMediaStream::RemoveMediaListener(listener);
	//Call parent
	if (num==0)
		//Stop sending
		StopSending();
	//Return number of listeners
	return num;
}

void RTMPParticipant::RemoveAllMediaListeners()
{
	//Call parent
	RTMPMediaStream::RemoveAllMediaListeners();
	//Stop sending
	StopSending();
}
void RTMPParticipant::onAttached(RTMPMediaStream *stream)
{
	Log("-RTMP participant attached to stream [id:%d]\n",stream?stream->GetStreamId():-1);

	//Check if it is the same
	if (stream==attached)
	{
		//Error
		Error("Already attached to same string\n");
		//Do nothing
		return;
	}
	//Check if already attached
	if (attached)
		//Remove from that listeners
		attached->RemoveMediaListener(this);
	//Store stream
	attached = stream;
	
	//Start receiving
	StartReceiving();
}

void RTMPParticipant::onMediaFrame(DWORD id,RTMPMediaFrame *frame)
{
	//Depending on the type
	switch (frame->GetType())
	{
		case RTMPMediaFrame::Video:
			//Push it
			videoFrames.Add((RTMPVideoFrame*)(frame->Clone()));
			break;
		case RTMPMediaFrame::Audio:
			//Convert to audio and push
			audioFrames.Add((RTMPAudioFrame*)(frame->Clone()));
			break;
	}
}

void RTMPParticipant::onMetaData(DWORD id,RTMPMetaData *publishedMetaData)
{
	//Cheeck
	if (!sendingText)
		//Exit
		return;

	//Get name
	AMFString* name = (AMFString*)publishedMetaData->GetParams(0);

	//Check it the send command text
	if (name->GetWString().compare(L"onText")==0)
	{
		//Get string
		AMFString *str = (AMFString*)publishedMetaData->GetParams(1);

		//Log
		Log("Sending t140 data [\"%ls\"]\n",str->GetWChar());

		//Create frame
		TextFrame frame(getDifTime(&first)/1000,str->GetWString());
		//Send text
		textOutput->SendFrame(frame);
	}
}

void RTMPParticipant::onCommand(DWORD id,const wchar_t *name,AMFData* obj)
{

}

void RTMPParticipant::onStreamBegin(DWORD id)
{

}

void RTMPParticipant::onStreamEnd(DWORD id)
{

}

void RTMPParticipant::onStreamReset(DWORD id)
{

}

void RTMPParticipant::onDetached(RTMPMediaStream *stream)
{
	Log("-RTMP participant detached from stream [id:%d]\n",stream?stream->GetStreamId():-1);

	//Check if already attached
	if (attached!=stream)
		//Exit
		return;
	//Not attached anymore
	attached = NULL;
	//Start receiving
	StopReceiving();
}

