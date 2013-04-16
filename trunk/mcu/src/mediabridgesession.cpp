/* 
 * File:   mediabridgesession.cpp
 * Author: Sergio
 * 
 * Created on 22 de diciembre de 2010, 18:20
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
#include "mediabridgesession.h"
#include "codecs.h"
#include "rtpsession.h"
#include "video.h"
#include "h263/h263codec.h"
#include "flv1/flv1codec.h"
#include "avcdescriptor.h"
#include "fifo.h"
#include <wchar.h>
#include <string>

#ifdef FLV1PARSER
#include "flv1/flv1Parser.h"
#endif
extern DWORD  h264_append_nals(BYTE *dest, DWORD destLen, DWORD destSize, BYTE *buffer, DWORD bufferLen,BYTE **nals,DWORD nalSize,DWORD *num);

MediaBridgeSession::MediaBridgeSession() : rtpAudio(MediaFrame::Audio,NULL), rtpVideo(MediaFrame::Video,NULL), rtpText(MediaFrame::Text,NULL)
{
	//Neither sending nor receiving
	sendingAudio = false;
	sendingVideo = false;
	sendingText = false;
	receivingAudio = false;
	receivingVideo = false;
	receivingText = false;
	sendFPU = false;
	//Neither transmitter nor receiver
	meta = NULL;
	inited = false;
	//Default values
	rtpVideoCodec = VideoCodec::H264;
	rtpAudioCodec = AudioCodec::PCMU;
	//Create default encoders and decoders
	rtpAudioEncoder = AudioCodecFactory::CreateEncoder(AudioCodec::PCMU);
	rtpAudioDecoder = AudioCodecFactory::CreateDecoder(AudioCodec::PCMU);
	//Create speex encoder and decoder
	rtmpAudioEncoder = AudioCodecFactory::CreateEncoder(AudioCodec::NELLY11);
	rtmpAudioDecoder = AudioCodecFactory::CreateDecoder(AudioCodec::NELLY11);

	//Inicializamos los mutex
	pthread_mutex_init(&mutex,NULL);
}

MediaBridgeSession::~MediaBridgeSession()
{
	//End it just in case
	End();
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
	//Delete codecs
	if (rtpAudioEncoder)
		delete(rtpAudioEncoder);
	if (rtpAudioDecoder)
		delete(rtpAudioDecoder);
	if (rtmpAudioEncoder)
		delete(rtmpAudioEncoder);
	if (rtmpAudioDecoder)
		delete(rtmpAudioDecoder);
}

bool MediaBridgeSession::Init()
{
	//We are started
	inited = true;
	//Wait for first Iframe
	waitVideo = true;
	//Init rtp
	rtpAudio.Init();
	rtpVideo.Init();
	rtpText.Init();
	//Init smoother for video
	smoother.Init(&rtpVideo);
	//Set first timestamp
	getUpdDifTime(&first);

	return true;
}

bool MediaBridgeSession::End()
{
	//Check if we are running
	if (!inited)
		return false;

	Log(">MediaBridgeSession end\n");

	//Stop everithing
	StopReceivingAudio();
	StopReceivingVideo();
	StopReceivingText();
	StopSendingAudio();
	StopSendingVideo();
	StopSendingText();

	//Remove meta
	if (meta)
		//delete objet
		delete(meta);
	//No meta
	meta = NULL;

	//Close smoother
	smoother.End();

	//End rtp
	rtpAudio.End();
	rtpVideo.End();
	rtpText.End();

	//Stop
	inited=0;

	Log("<MediaBridgeSession end\n");

	return true;
}

int  MediaBridgeSession::StartSendingVideo(char *sendVideoIp,int sendVideoPort,RTPMap& rtpMap)
{
	Log("-StartSendingVideo [%s,%d]\n",sendVideoIp,sendVideoPort);

	//Si estabamos mandando tenemos que parar
	if (sendingVideo)
		//Y esperamos que salga
		StopSendingVideo();

	//Si tenemos video
	if (sendVideoPort==0)
		return Error("No video port defined\n");

	//Iniciamos las sesiones rtp de envio
	if(!rtpVideo.SetRemotePort(sendVideoIp,sendVideoPort))
		//Error
		return Error("Error abriendo puerto rtp\n");

	//Set sending map
	rtpVideo.SetSendingRTPMap(rtpMap);

	//Estamos mandando
	sendingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&sendVideoThread,startSendingVideo,this,0);

	return sendingVideo;
}

int  MediaBridgeSession::StopSendingVideo()
{
	Log(">StopSendingVideo\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (sendingVideo)
	{
		//Dejamos de recivir
		sendingVideo=0;

		//Cancel any pending wait
		videoFrames.Cancel();

		//Esperamos
		pthread_join(sendVideoThread,NULL);
	}

	return true;

	Log("<StopSendingVideo\n");
}

int  MediaBridgeSession::StartReceivingVideo(RTPMap& rtpMap)
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingVideo)
		StopReceivingVideo();

	//Iniciamos las sesiones rtp de recepcion
	int recVideoPort= rtpVideo.GetLocalPort();

	//Set receving map
	rtpVideo.SetReceivingRTPMap(rtpMap);

	//Estamos recibiendo
	receivingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&recVideoThread,startReceivingVideo,this,0);

	//Logeamos
	Log("-StartReceivingVideo [%d]\n",recVideoPort);

	return recVideoPort;
}

int  MediaBridgeSession::StopReceivingVideo()
{
	Log(">StopReceivingVideo\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingVideo)
	{
		//Dejamos de recivir
		receivingVideo=0;

		//Cancel the rtp
		rtpVideo.CancelGetPacket();

		//Esperamos
		pthread_join(recVideoThread,NULL);
	}

	Log("<StopReceivingVideo\n");

	return 1;
}

int  MediaBridgeSession::StartSendingAudio(char *sendAudioIp,int sendAudioPort,RTPMap& rtpMap)
{
	Log("-StartSendingAudio [%s,%d]\n",sendAudioIp,sendAudioPort);

	//Si estabamos mandando tenemos que parar
	if (sendingAudio)
		//Y esperamos que salga
		StopSendingAudio();

	//Si tenemos Audio
	if (sendAudioPort==0)
		return Error("No Audio port defined\n");

	//Iniciamos las sesiones rtp de envio
	if(!rtpAudio.SetRemotePort(sendAudioIp,sendAudioPort))
		return Error("Error abriendo puerto rtp\n");

	//Set sending map
	rtpAudio.SetSendingRTPMap(rtpMap);

	//Set default codec
	rtpAudio.SetSendingCodec(rtpAudioCodec);

	//Estamos mandando
	sendingAudio=1;

	//Arrancamos los procesos
	createPriorityThread(&sendAudioThread,startSendingAudio,this,0);

	return sendingAudio;
}

int  MediaBridgeSession::StopSendingAudio()
{
	Log(">StopSendingAudio\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (sendingAudio)
	{
		//Dejamos de recivir
		sendingAudio=0;

		//Cancel any pending wait
		audioFrames.Cancel();

		//Esperamos
		pthread_join(sendAudioThread,NULL);
	}

	Log("<StopSendingAudio\n");

	return 1;
}

int  MediaBridgeSession::StartReceivingAudio(RTPMap& rtpMap)
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingAudio)
		StopReceivingAudio();

	//Iniciamos las sesiones rtp de recepcion
	int recAudioPort= rtpAudio.GetLocalPort();

	//Set receving map
	rtpAudio.SetReceivingRTPMap(rtpMap);

	//Estamos recibiendo
	receivingAudio=1;

	//Arrancamos los procesos
	createPriorityThread(&recAudioThread,startReceivingAudio,this,0);

	//Logeamos
	Log("-StartReceivingAudio [%d]\n",recAudioPort);

	return recAudioPort;
}

int  MediaBridgeSession::StopReceivingAudio()
{
	Log(">StopReceivingAudio\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingAudio)
	{
		//Dejamos de recivir
		receivingAudio=0;

		//Cancel audio rtp get packet
		rtpAudio.CancelGetPacket();
		
		//Esperamos
		pthread_join(recAudioThread,NULL);
	}

	Log("<StopReceivingAudio\n");

	return 1;
}

int  MediaBridgeSession::StartSendingText(char *sendTextIp,int sendTextPort,RTPMap& rtpMap)
{
	Log("-StartSendingTextt [%s,%d]\n",sendTextIp,sendTextPort);

	//Si estabamos mandando tenemos que parar
	if (sendingText)
		//Y esperamos que salga
		StopSendingText();

	//Si tenemos Text
	if (sendTextPort==0)
		return Error("No Text port defined\n");

	//Set sending map
	rtpText.SetSendingRTPMap(rtpMap);

	//Set codec type
	rtpText.SetSendingCodec(TextCodec::T140);

	//Iniciamos las sesiones rtp de envio
	if(!rtpText.SetRemotePort(sendTextIp,sendTextPort))
		//Error
		return Error("Error abriendo puerto rtp\n");

	//Estamos mandando
	sendingText=1;

	return sendingText;
}

int  MediaBridgeSession::StopSendingText()
{
	Log(">StopSendingText\n");

	//Check
	if (sendingText)
	{
		//Dejamos de recivir
		sendingText=0;
	}

	Log("<StopSendingText\n");

	return 1;
}

int  MediaBridgeSession::StartReceivingText(RTPMap& rtpMap)
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingText)
		StopReceivingText();

	//Iniciamos las sesiones rtp de recepcion
	int recTextPort = rtpText.GetLocalPort();

	//Estamos recibiendo
	receivingText = 1;

	//Set receving map
	rtpText.SetReceivingRTPMap(rtpMap);

	//Arrancamos los procesos
	createPriorityThread(&recTextThread,startReceivingText,this,0);

	//Logeamos
	Log("-StartReceivingText [%d]\n",recTextPort);

	return recTextPort;
}

int  MediaBridgeSession::StopReceivingText()
{
	Log(">StopReceivingText\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingText)
	{
		//Dejamos de recivir
		receivingText=0;

		//Cancel rtp
		rtpText.CancelGetPacket();

		//Esperamos
		pthread_join(recTextThread,NULL);
	}

	Log("<StopReceivingText\n");

	return 1;
}

/**************************************
* startReceivingVideo
*	Function helper for thread
**************************************/
void* MediaBridgeSession::startReceivingVideo(void *par)
{
	Log("RecVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	MediaBridgeSession *sess = (MediaBridgeSession *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->RecVideo());
}

/**************************************
* startReceivingAudio
*	Function helper for thread
**************************************/
void* MediaBridgeSession::startReceivingAudio(void *par)
{
	Log("RecVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	MediaBridgeSession *sess = (MediaBridgeSession *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->RecAudio());
}

/**************************************
* startSendingVideo
*	Function helper for thread
**************************************/
void* MediaBridgeSession::startSendingVideo(void *par)
{
	Log("SendVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	MediaBridgeSession *sess = (MediaBridgeSession *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->SendVideo());
}

/**************************************
* startSendingAudio
*	Function helper for thread
**************************************/
void* MediaBridgeSession::startSendingAudio(void *par)
{
	Log("SendAudioThread [%d]\n",getpid());

	//Obtenemos el objeto
	MediaBridgeSession *sess = (MediaBridgeSession *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->SendAudio());
}

/**************************************
* startReceivingText
*	Function helper for thread
**************************************/
void* MediaBridgeSession::startReceivingText(void *par)
{
	Log("RecTextThread [%d]\n",getpid());

	//Obtenemos el objeto
	MediaBridgeSession *sess = (MediaBridgeSession *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)sess->RecText());
}

/****************************************
* RecVideo
*	Obtiene los packetes y los muestra
*****************************************/
int MediaBridgeSession::RecVideo()
{
	//Coders
	VideoDecoder* decoder = NULL;
	VideoEncoder* encoder = VideoCodecFactory::CreateEncoder(VideoCodec::SORENSON);
	//Create new video frame
	RTMPVideoFrame  frame(0,262143);
	//Set codec
	frame.SetVideoCodec(RTMPVideoFrame::FLV1);

	int 	width=0;
	int 	height=0;
	DWORD	numpixels=0;
	
	Log(">RecVideo\n");

	//Mientras tengamos que capturar
	while(receivingVideo)
	{
		///Obtenemos el paquete
		RTPPacket* packet = rtpVideo.GetPacket();

		//Check
		if (!packet)
			//Next
			continue;

		//Get type
		VideoCodec::Type type = (VideoCodec::Type)packet->GetCodec();


		if ((decoder==NULL) || (type!=decoder->type))
		{
			//Si habia uno nos lo cargamos
			if (decoder!=NULL)
				delete decoder;

			//Creamos uno dependiendo del tipo
			decoder = VideoCodecFactory::CreateDecoder(type);

			//Check
			if (!decoder)
				continue;
		}

		//Lo decodificamos
		if(!decoder->DecodePacket(packet->GetMediaData(),packet->GetMediaLength(),0,packet->GetMark()))
		{
			delete(packet);
			continue;
		}
		//Get mark
		bool mark = packet->GetMark();

		//Delete packet
		delete(packet);

		//Check if it is last one
		if(!mark)
			continue;
	
		//Check size
		if (decoder->GetWidth()!=width || decoder->GetHeight()!=height)
		{
			//Get dimension
			width = decoder->GetWidth();
			height = decoder->GetHeight();

			//Set size
			numpixels = width*height*3/2;

			//Set also frame rate and bps
			encoder->SetFrameRate(25,300,500);

			//Set them in the encoder
			encoder->SetSize(width,height);
		}

		//Encode next frame
		VideoFrame *encoded = encoder->EncodeFrame(decoder->GetFrame(),numpixels);

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

		//Check type
		if (encoded->IsIntra())
			//Set type
			frame.SetFrameType(RTMPVideoFrame::INTRA);
		else
			//Set type
			frame.SetFrameType(RTMPVideoFrame::INTER);

		//Let the connection set the timestamp
		frame.SetTimestamp(getDifTime(&first)/1000);

		//Send it
		SendMediaFrame(&frame);
	}

	//Check
	if (decoder)
		//Delete
		delete(decoder);
	//Check
	if (encoder)
		//Delete
		delete(encoder);

	Log("<RecVideo\n");

	//Salimos
	pthread_exit(0);
}

/****************************************
* RecAudio
*	Obtiene los packetes y los muestra
*****************************************/
int MediaBridgeSession::RecAudio()
{
	DWORD		firstAudio = 0;
	DWORD		timeStamp=0;
	DWORD		firstTS = 0;
	SWORD		raw[512];
	DWORD		rawSize = 512;
	DWORD		rawLen;

	//Create new audio frame
	RTMPAudioFrame  *audio = new RTMPAudioFrame(0,MTU);

	Log(">RecAudio\n");

	//Mientras tengamos que capturar
	while(receivingAudio)
	{
		//Obtenemos el paquete
		RTPPacket *packet = rtpAudio.GetPacket();
		
		//Check
		if (!packet)
			//Next
			continue;
		
		//Get type
		AudioCodec::Type codec = (AudioCodec::Type)packet->GetCodec();

		//Check rtp type
		if (codec==AudioCodec::SPEEX16)
		{
			//TODO!!!!
		}

		//Check if we have a decoder
		if (!rtpAudioDecoder || rtpAudioDecoder->type!=codec)
		{
			//Check
			if (rtpAudioDecoder)
				//Delete old one
				delete(rtpAudioDecoder);
			//Create new one
			rtpAudioDecoder = AudioCodecFactory::CreateDecoder(codec);
		}

		//Decode it
		rawLen = rtpAudioDecoder->Decode(packet->GetMediaData(),packet->GetMediaLength(),raw,rawSize);

		//Delete packet
		delete(packet);

		//Rencode it
		DWORD len;
		
		while((len=rtmpAudioEncoder->Encode(raw,rawLen,audio->GetMediaData(),audio->GetMaxMediaSize()))>0)
		{
			//REset
			rawLen = 0;
			
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

			//If it is first
			if (!firstTS)
			{
				//Get first audio time
				firstAudio = getDifTime(&first)/1000;
				//It is first
				firstTS = timeStamp;
			}

			DWORD ts = firstAudio +(timeStamp-firstTS)/8;
			//Set timestamp
			audio->SetTimestamp(ts);

			//Send packet
			SendMediaFrame(audio);
		}
	}

	//Check
	if (audio)
		//Delete it
		delete(audio);
	
	Log("<RecAudio\n");

	//Salimos
	pthread_exit(0);
}

/****************************************
* RecText
*	Obtiene los packetes y los muestra
*****************************************/
int MediaBridgeSession::RecText()
{
	DWORD		timeStamp=0;
	
	Log(">RecText\n");

	//Mientras tengamos que capturar
	while(receivingText)
	{
		TextCodec::Type type;
		DWORD packetSize = MTU;

		//Get packet
		RTPPacket *packet = rtpText.GetPacket();

		//Check packet
		if (!packet)
			continue;

		WORD skip = 0;

		//Get data
		BYTE* data = packet->GetMediaData();
		//And length
		DWORD size = packet->GetMediaLength();
		
		//Check the type of data
		if (type==TextCodec::T140RED)
		{
			WORD i = 0;
			bool last = false;

			//Read redundant headers
			while(!last)
			{
				//Check if it is the last
				last = !(data[i++]>>7);
				//if it is not last
				if (!last)
				{
					//Get offset
					WORD offset	= (((WORD)(data[i++])<<6));
					offset |= data[i]>>2;
					WORD size	= (((WORD)(data[i++])&0x03)<<8);
					size |= data[i++];
					//Skip the redundant data
					skip += size;
				}
			}
			//Skip redundant data
			skip += i;
		}



		//Check length
		if (skip<size)
	        {
			//Create frame
			TextFrame frame(timeStamp,data+skip,size-skip);

			//Create new timestamp associated to latest media time
			RTMPMetaData meta(getDifTime(&first)/1000);

			//Add text name
			meta.AddParam(new AMFString(L"onText"));
			//Set data
			meta.AddParam(new AMFString(frame.GetWChar()));

			//Send data
			SendMetaData(&meta);
		}
	}

	Log("<RecText\n");

	//Salimos
	pthread_exit(0);
}


int MediaBridgeSession::SetSendingVideoCodec(VideoCodec::Type codec)
{
	//Store it
	rtpVideoCodec = codec;
	//OK
	return 1;
}

int MediaBridgeSession::SetSendingAudioCodec(AudioCodec::Type codec)
{
	//Store it
	rtpAudioCodec = codec;
	//OK
	return 1;
}

int MediaBridgeSession::SendFPU()
{
	//Set it
	sendFPU = true;

	//Exit
	return 1;
}

int MediaBridgeSession::SendVideo()
{
	VideoDecoder *decoder = VideoCodecFactory::CreateDecoder(VideoCodec::SORENSON);
	VideoEncoder *encoder = VideoCodecFactory::CreateEncoder(rtpVideoCodec);
	DWORD width = 0;
	DWORD height = 0;
	DWORD numpixels = 0;
	
	QWORD	lastVideoTs = 0;
	
	Log(">SendVideo\n");

	//Set video format
	if (!rtpVideo.SetSendingCodec(rtpVideoCodec))
		//Error
		return Error("Peer do not support [%d,%s]\n",rtpVideoCodec,VideoCodec::GetNameFor(rtpVideoCodec));

	//While sending video
	while (sendingVideo)
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

		//Get time difference
		DWORD diff = 0;
		//Get timestam
		QWORD ts = video->GetTimestamp();
		//If it is not the first frame
		if (lastVideoTs)
			//Calculate it
			diff = ts - lastVideoTs;
		//Set the last audio timestamp
		lastVideoTs = ts;

		//Check
		if (video->GetVideoCodec()!=RTMPVideoFrame::FLV1)
			//Error
			continue;

		//Decode frame
		if (!decoder->Decode(video->GetMediaData(),video->GetMediaSize()))
		{
			Error("decode packet error");
			//Next
			continue;
		}

		//Check size
		if (decoder->GetWidth()!=width || decoder->GetHeight()!=height)
		{
			//Get dimension
			width = decoder->GetWidth();
			height = decoder->GetHeight();

			//Set size
			numpixels = width*height*3/2;

			//Set also frame rate and bps
			encoder->SetFrameRate(25,300,500);

			//Set them in the encoder
			encoder->SetSize(width,height);
		}
		//Check size
		if (!numpixels)
		{
			Error("numpixels equals 0");
			//Next
			continue;
		}
		//Check fpu
		if (sendFPU)
		{
			//Send it
			encoder->FastPictureUpdate();
			//Reset
			sendFPU = false;
		}

		//Encode it
		VideoFrame *videoFrame = encoder->EncodeFrame(decoder->GetFrame(),numpixels);

		//If was failed
		if (!videoFrame)
		{
			Log("No video frame\n");
			//Next
			continue;
		}

		//Set frame time
		videoFrame->SetTimestamp(diff);

		//Send it smoothly
		smoother.SendFrame(videoFrame,diff);

		//Delete video frame
		delete(video);
	}

	Log("<SendVideo\n");

	return 1;
}

int MediaBridgeSession::SendAudio()
{
	AudioCodec::Type rtmpAudioCodec;
	QWORD lastAudioTs = 0;

	Log(">SendAudio\n");

	//While sending audio
	while (sendingAudio)
	{
		//Wait for next audio
		if (!audioFrames.Wait(0))
			//Check again
			continue;

		//Get audio grame
		RTMPAudioFrame* audio = audioFrames.Pop();
		//check
		if (!audio)
			//Again
			continue;
		//Get timestamp
		QWORD ts = audio->GetTimestamp();
		//Get delay
		QWORD diff = 0;
		//If it is the first frame
		if (lastAudioTs)
			//Calculate it
			diff = ts - lastAudioTs;
		//Check diff
		if (diff<40)
			//Set it to only one frame of 20 ms
			diff = 20;
		//Set the last audio timestamp
		lastAudioTs = ts;
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
		//Check rtp type
		if (rtpAudioCodec!=rtmpAudioCodec)
		{
			BYTE rtp[MTU];
			DWORD rtpSize = MTU;
			DWORD rtpLen = 0;
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
				rtmpAudioDecoder = AudioCodecFactory::CreateDecoder(rtmpAudioCodec);
			}
			//Get data
			BYTE *data = audio->GetMediaData();
			//Get size
			DWORD size = audio->GetMediaSize();
			//Decode it until no frame is found
			while ((rawLen = rtmpAudioDecoder->Decode(data,size,raw,rawSize))>0)
			{
				//Check size
				if (rawLen>0)
				{
					//Rencode ig
					rtpLen = rtpAudioEncoder->Encode(raw,rawLen,rtp,rtpSize);
					//Send
					if (rtpAudioCodec==AudioCodec::SPEEX16)
					{
						//Send rtp packet
						//FIX!!  rtpAudio.SendAudioPacket(rtp,rtpLen,diff*16);
					} else {
						//Send rtp packet
						//FIX!! rtpAudio.SendAudioPacket(rtp,rtpLen,diff*8);
					}
				}
				//Set diff
				diff = 20;
				//Remove size
				size = 0;
			}
		} else {
			//Send rtp packet
			//FIX!!  rtpAudio.SendAudioPacket(audio->GetMediaData(),audio->GetMediaSize(),diff*16);
		}
		//Delete audio
		delete(audio);
	}

	Log("<SendAudio\n");

	return 1;
}

#if 0
/****************************************
* RecVideo
*	Obtiene los packetes y los muestra
*****************************************/
int MediaBridgeSession::RecVideo()
{
	BYTE 	lost=0;
	BYTE 	last=0;
	DWORD	timeStamp=0;
	DWORD	firstVideo=0;
	DWORD	firstTS=0;
	int 	width=0;
	int 	height=0;
	bool	isIntra = false;

	//RTP incoming packet
	DWORD packetSize = MTU;
	BYTE* packet = (BYTE*)malloc(packetSize);


	//Create new video frame
	RTMPVideoFrame  *video = new RTMPVideoFrame(0,65500);

	//Temporary buffer
	DWORD bufferSize = video->GetMaxMediaSize();
	BYTE* buffer =  video->GetMediaData();
	WORD bufferLen = 0;


	//The descriptor for the AVC
	AVCDescriptor desc;
	//Set description properties
	desc.SetConfigurationVersion(0x01);
	desc.SetAVCProfileIndication(0x42);	//Baseline
	desc.SetProfileCompatibility(0xC0);
	desc.SetAVCLevelIndication(0x0D);	//1.3
	desc.SetNALUnitLength(4);

	Log(">RecVideo\n");

	//Mientras tengamos que capturar
	while(receivingVideo)
	{
		//Video codec type
		VideoCodec::Type type;

		//POnemos el tam�o
		packetSize=MTU;

		//Obtenemos el paquete
		if (!rtpVideo.GetVideoPacket((BYTE *)packet,&packetSize,&last,&lost,&type,&timeStamp))
		{
			Error("Error recv video [%d]\n",errno);
			continue;
		}

		//If it is first
		if (!firstTS)
		{
			//Get first audio time
			firstVideo = getDifTime(&first)/1000;
			//It is first
			firstTS = timeStamp;
		}

		DWORD ts = firstVideo +(timeStamp-firstTS)/90;

		//Depending on the video type
		switch (type)
		{
			case VideoCodec::H263_1998:
				break;
			case VideoCodec::H263_1996:
				break;
			case VideoCodec::H264:
			{
				//Nals
				BYTE* nals[16];
				DWORD num;

				//Depacketize it
				bufferLen += h264_append_nals(buffer,bufferLen,bufferSize,packet,packetSize,(BYTE**)&nals,16,&num);

				//For each nal
				for (DWORD i=0;i<num;i++)
				{

					DWORD nalSize = 0;
					//Get nal
					BYTE *nal = nals[i];
					//Get nal type
					BYTE nalType = (nal[0] & 0x1f);
					//Get length
					if (i<num-1)
						//Get nal size to the next nal
						nalSize = nals[i+1]-nals[i];
					else
						//Get nal size to the end of buffer
						nalSize = bufferLen - (nals[i]-buffer);
					//Depending on the type
					switch (nalType)
					{
						case 1:
							break;
						case 6:
							break;
						case 7:
							//Clear SPS
							desc.ClearSequenceParameterSets();
							//Add new SPS
							desc.AddSequenceParameterSet(nal,nalSize);
							break;
						case 8:
							//Clear PPS
							desc.ClearPictureParameterSets();
							//Add PPS
							desc.AddPictureParameterSet(nal,nalSize);
							break;
						case 5:
							//Is intra
							isIntra = true;
							break;
						default:
							Debug("Unhandled nal [%d]\n",nalType);
					}
				}
				//If it is not last
				if (!last)
					//Get next packet
					continue;

				//Set frame type
				video->SetVideoCodec(RTMPVideoFrame::AVC);
				//Set NALU type
				video->SetAVCType(1);
				//No delay
				video->SetAVCTS(0);

				//Set buffer len
				video->SetMediaSize(bufferLen);
				//If we have the receiver
				if (receiver)
				{
					//If it is intra
					if (isIntra)
					{
						//Set type
						video->SetFrameType(RTMPVideoFrame::INTRA);

						//If we where waiting for video
						if (waitVideo)
						{
							//Stop waiting
							waitVideo = false;
							getUpdDifTime(&first); //!!
						}

						//Create rtmp frame to send descriptor
						RTMPVideoFrame *frame = new RTMPVideoFrame(0,desc.GetSize());
						//Set type
						frame->SetVideoCodec(RTMPVideoFrame::AVC);
						//Set type
						frame->SetFrameType(RTMPVideoFrame::INTRA);
						//Set NALU type
						frame->SetAVCType(0);
						//Set no delay
						frame->SetAVCTS(0);
						//Serialize
						DWORD len = desc.Serialize(frame->GetMediaData(),frame->GetMaxMediaSize());
						//Set size
						frame->SetMediaSize(len);
						//Set timestamp
						frame->SetTimestamp(getDifTime(&first)/1000);
						//Send it
						receiver->PlayMediaFrame(frame);
						//Delete it
						delete(frame);
					} else {
						//Set type
						video->SetFrameType(RTMPVideoFrame::INTER);
					}
					//Set timestamp
					video->SetTimestamp(getDifTime(&first)/1000);
					//Check if we are still waiting for the intra
					if (!waitVideo)
						//Send it
						receiver->PlayMediaFrame(video);
				}
				//No size yet
				bufferLen = 0;
				//No intra
				isIntra = false;
				break;
			}
		}
	}

	//Delete
	delete(video);
	//Free rtp packet
	free(packet);

	Log("<RecVideo\n");

	//Salimos
	pthread_exit(0);
}
#endif
void MediaBridgeSession::AddInputToken(const std::wstring &token)
{
	//Add token
	inputTokens.insert(token);
}
void MediaBridgeSession:: AddOutputToken(const std::wstring &token)
{
	//Add token
	outputTokens.insert(token);
}

bool MediaBridgeSession::ConsumeInputToken(const std::wstring &token)
{
	//Check token
	ParticipantTokens::iterator it = inputTokens.find(token);

	//Check we found one
	if (it==inputTokens.end())
		//Not found
		return Error("Participant token not found\n");

	//Remove token
	inputTokens.erase(it);

	//Ok
	return true;
}

bool MediaBridgeSession::ConsumeOutputToken(const std::wstring &token)
{
	//Check token
	ParticipantTokens::iterator it = outputTokens.find(token);

	//Check we found one
	if (it==outputTokens.end())
		//Not found
		return Error("Participant token not found\n");

	//Remove token
	outputTokens.erase(it);

	//Ok
	return true;
}

DWORD MediaBridgeSession::AddMediaListener(RTMPMediaStream::Listener *listener)
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

	//Init
	listener->onStreamBegin(RTMPMediaStream::id);

	//If we already have metadata
	if (meta)
		//Send it
		listener->onMetaData(RTMPMediaStream::id,meta);

	//Return number of listeners
	return num;
}

DWORD MediaBridgeSession::RemoveMediaListener(RTMPMediaStream::Listener *listener)
{
	//Call parent
	DWORD num = RTMPMediaStream::RemoveMediaListener(listener);

	//Return number of listeners
	return num;
}

void MediaBridgeSession::onAttached(RTMPMediaStream *stream)
{
	Log("-RTMP media bridged attached to stream [id:%d]\n",stream?stream->GetStreamId():-1);

}

void MediaBridgeSession::onMediaFrame(DWORD id,RTMPMediaFrame *frame)
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

void MediaBridgeSession::onMetaData(DWORD id,RTMPMetaData *publishedMetaData)
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
		RTPPacket packet(MediaFrame::Text,TextCodec::T140);
		
		//Get string
		AMFString *str = (AMFString*)publishedMetaData->GetParams(1);

		//Log
		Log("Sending t140 data [\"%ls\"]\n",str->GetWChar());

		//Set data
		packet.SetPayload((BYTE*)str->GetWChar(),str->GetUTF8Size());

		//Set timestamp
		packet.SetTimestamp(getDifTime(&first)/1000);

		//No mark
		packet.SetMark(false);

		//Send it
		rtpText.SendPacket(packet);
	}
}

void MediaBridgeSession::onCommand(DWORD id,const wchar_t *name,AMFData* obj)
{

}

void MediaBridgeSession::onStreamBegin(DWORD id)
{

}

void MediaBridgeSession::onStreamEnd(DWORD id)
{

}

void MediaBridgeSession::onStreamReset(DWORD id)
{

}

void MediaBridgeSession::onDetached(RTMPMediaStream *stream)
{
	Log("-RTMP media bridge detached from stream [id:%d]\n",stream?stream->GetStreamId():-1);
}

/*****************************************************
 * RTMP NetStream
 *
 ******************************************************/
MediaBridgeSession::NetStream::NetStream(DWORD streamId,MediaBridgeSession *sess,RTMPNetStream::Listener* listener) : RTMPNetStream(streamId,listener)
{
	//Store conf
	this->sess = sess;
}

MediaBridgeSession::NetStream::~NetStream()
{
	//Close
	Close();
	///Remove listener just in case
	RemoveAllMediaListeners();
}

void MediaBridgeSession::NetStream::doPlay(std::wstring& url,RTMPMediaStream::Listener* listener)
{
	//Log
	Log("-Play stream [%ls]\n",url.c_str());

	//Remove extra data from FMS
	if (url.find(L"*flv:")==0)
		//Erase it
		url.erase(0,5);
	else if (url.find(L"flv:")==0)
		//Erase it
		url.erase(0,4);

	//Check token
	if (!sess->ConsumeOutputToken(url))
	{
		//Send error
		fireOnNetStreamStatus(RTMP::Netstream::Play::StreamNotFound,L"Token invalid");
		//Exit
		return;
	}

	//Send reseted status
	fireOnNetStreamStatus(RTMP::Netstream::Play::Reset,L"Playback reset");
	//Send play status
	fireOnNetStreamStatus(RTMP::Netstream::Play::Start,L"Playback started");

	//Add listener
	AddMediaListener(listener);
	//Attach
	Attach(sess);
}

/***************************************
 * Publish
 *	RTMP event listener
 **************************************/
void MediaBridgeSession::NetStream::doPublish(std::wstring& url)
{
	//Log
	Log("-Publish stream [%ls]\n",url.c_str());

	//Get participant stream
	if (!sess->ConsumeInputToken(url))
	{
		//Send error
		fireOnNetStreamStatus(RTMP::Netstream::Publish::BadName,L"Token invalid");
		//Exit
		return;
	}

	//Add this as listener
	AddMediaListener(sess);

	//Send publish notification
	fireOnNetStreamStatus(RTMP::Netstream::Publish::Start,L"Publish started");
	
}

/***************************************
 * Close
 *	RTMP event listener
 **************************************/
void MediaBridgeSession::NetStream::doClose(RTMPMediaStream::Listener *listener)
{
	//REmove listener
	RemoveMediaListener(listener);
	//Close
	Close();
}

void MediaBridgeSession::NetStream::NetStream::Close()
{
	Log(">Close mediabridge netstream\n");

	//Dettach if playing
	Detach();

	Log("<Closed\n");
}

/********************************
 * NetConnection
 **********************************/
RTMPNetStream* MediaBridgeSession::CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener *listener)
{
	//No stream for that url
	RTMPNetStream *stream = new NetStream(streamId,this,listener);

	//Register the sream
	RegisterStream(stream);

	//Create stream
	return stream;
}

void MediaBridgeSession::DeleteStream(RTMPNetStream *stream)
{
	//Unregister stream
	UnRegisterStream(stream);

	//Delete the stream
	delete(stream);
}
