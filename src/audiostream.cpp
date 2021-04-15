#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include "log.h"
#include "tools.h"
#include "audio.h"
#include "audiostream.h"
#include "mp4recorder.h"
#include "AudioCodecFactory.h"


/**********************************
* AudioStream
*	Constructor
***********************************/
AudioStream::AudioStream(RTPSession::Listener* listener) : rtp(MediaFrame::Audio,listener)
{
	audioInput = NULL;
	audioOutput = NULL;
	sendingAudio=0;
	receivingAudio=0;
	audioCodec=AudioCodec::PCMU;
	muted = 0;
}

/*******************************
* ~AudioStream
*	Destructor. 
********************************/
AudioStream::~AudioStream()
{
}

/***************************************
* SetAudioCodec
*	Fija el codec de audio
***************************************/
int AudioStream::SetAudioCodec(AudioCodec::Type codec,const Properties& properties)
{
	//Colocamos el tipo de audio
	audioCodec = codec;

	//Store properties
	audioProperties = properties;

	Log("-SetAudioCodec [%d,%s]\n",audioCodec,AudioCodec::GetNameFor(audioCodec));

	//Y salimos
	return 1;	
}


/***************************************
* Init
*	Inicializa los devices 
***************************************/
int AudioStream::Init(AudioInput *input, AudioOutput *output)
{
	Log(">Init audio stream\n");

	//Iniciamos el rtp
	if(!rtp.Init())
		return Error("Could not open rtp\n");
	
	//Nos quedamos con los puntericos
	audioInput  = input;
	audioOutput = output;

	//Y aun no estamos mandando nada
	sendingAudio=0;
	receivingAudio=0;
	
	//The time of init
	gettimeofday(&ini,NULL);

	Log("<Init audio stream\n");

	return 1;
}

int AudioStream::SetLocalCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetLocalCryptoSDES(suite,key64);
}

int AudioStream::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetRemoteCryptoSDES(suite,key64);
}

int AudioStream::SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint)
{
	return rtp.SetRemoteCryptoDTLS(setup,hash,fingerprint);
}

int AudioStream::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetLocalSTUNCredentials(username,pwd);
}

int AudioStream::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetRemoteSTUNCredentials(username,pwd);
}

int AudioStream::SetRTPProperties(const Properties& properties)
{
	return rtp.SetProperties(properties);
}
/***************************************
* startSendingAudio
*	Helper function
***************************************/
void * AudioStream::startSendingAudio(void *par)
{
	AudioStream *conf = (AudioStream *)par;
	blocksignals();
	Log("SendAudioThread [%p]\n",pthread_self());
	conf->SendAudio();
	//Exit
	return NULL;
}

/***************************************
* startReceivingAudio
*	Helper function
***************************************/
void * AudioStream::startReceivingAudio(void *par)
{
	AudioStream *conf = (AudioStream *)par;
	blocksignals();
	Log("RecvAudioThread [%p]\n",pthread_self());
	conf->RecAudio();
	//Exit
	return NULL;;
}

/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int AudioStream::StartSending(char *sendAudioIp,int sendAudioPort,const RTPMap& rtpMap,const RTPMap& aptMap)
{
	Log(">StartSending audio [%s,%d]\n",sendAudioIp,sendAudioPort);

	//Si estabamos mandando tenemos que parar
	if (sendingAudio)
		//paramos
		StopSending();
	
	//Si tenemos audio
	if (sendAudioPort==0)
		//Error
		return Error("Audio port 0\n");


	//Y la de audio
	if(!rtp.SetRemotePort(sendAudioIp,sendAudioPort))
		//Error
		return Error("Error en el SetRemotePort\n");

	//Set sending map
	rtp.SetSendingRTPMap(rtpMap,aptMap);

	//Set audio codec
	if(!rtp.SetSendingCodec(audioCodec))
		//Error
		return Error("%s audio codec not supported by peer\n",AudioCodec::GetNameFor(audioCodec));

	//Arrancamos el thread de envio
	sendingAudio=1;

	//Start thread
	createPriorityThread(&sendAudioThread,startSendingAudio,this,1);

	Log("<StartSending audio [%d]\n",sendingAudio);

	return 1;
}

/***************************************
* StartReceiving
*	Abre los sockets y empieza la recetpcion
****************************************/
int AudioStream::StartReceiving(const RTPMap& rtpMap,const RTPMap& aptMap)
{
	//If already receiving
	if (receivingAudio)
		//Stop it
		StopReceiving();
	
	//Get local rtp port
	int recAudioPort = rtp.GetLocalPort();

	//Set receving map
	rtp.SetReceivingRTPMap(rtpMap,aptMap);

	//We are reciving audio
	receivingAudio=1;

	//Create thread
	createPriorityThread(&recAudioThread,startReceivingAudio,this,1);

	//Log
	Log("<StartReceiving audio [%d]\n",recAudioPort);

	//Return receiving port
	return recAudioPort;
}

/***************************************
* End
*	Termina la conferencia activa
***************************************/
int AudioStream::End()
{
	//Terminamos de enviar
	StopSending();

	//Y de recivir
	StopReceiving();

	//Cerramos la session de rtp
	rtp.End();

	return 1;
}

/***************************************
* StopReceiving
* 	Termina la recepcion
****************************************/

int AudioStream::StopReceiving()
{
	Log(">StopReceiving Audio\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingAudio)
	{	
		//Paramos de enviar
		receivingAudio=0;

		//Cancel rtp
		rtp.CancelGetPacket();
		
		//Y unimos
		pthread_join(recAudioThread,NULL);
	}

	Log("<StopReceiving Audio\n");

	return 1;

}

/***************************************
* StopSending
* 	Termina el envio
****************************************/
int AudioStream::StopSending()
{
	Log(">StopSending Audio\n");

	//Esperamos a que se cierren las threads de envio
	if (sendingAudio)
	{
		//paramos
		sendingAudio=0;

		//Check audioInput
		if (audioInput)
			//Cancel
			audioInput->CancelRecBuffer();

		//Y esperamos
		pthread_join(sendAudioThread,NULL);
	}

	Log("<StopSending Audio\n");

	return 1;	
}


/****************************************
* RecAudio
*	Obtiene los packetes y los muestra
*****************************************/
int AudioStream::RecAudio()
{
	int 		recBytes=0;
	struct timeval 	before;
	SWORD		playBuffer[1024];
	const DWORD	playBufferSize = 1024;
	AudioDecoder*	codec=NULL;
	AudioCodec::Type type;
	QWORD		frameTime=0;
	QWORD		lastTime=0;
	
	Log(">RecAudio\n");
	
	//Inicializamos el tiempo
	gettimeofday(&before,NULL);

	//Mientras tengamos que capturar
	while(receivingAudio)
	{
		//Obtenemos el paquete
		auto packet = rtp.GetPacket();
		//Check
		if (!packet)
			//Next
			continue;
		
		//Get type
		type = (AudioCodec::Type)packet->GetCodec();

		//Comprobamos el tipo
		if ((codec==NULL) || (type!=codec->type))
		{
			//Si habia uno nos lo cargamos
			if (codec!=NULL)
				delete codec;

			//Creamos uno dependiendo del tipo
			if ((codec = AudioCodecFactory::CreateDecoder(type))==NULL)
			{
				//Next
				Log("Error creando nuevo codec de audio [%d]\n",type);
				continue;
			}

			//Try to set native pipe rate
			DWORD rate = codec->TrySetRate(audioOutput->GetNativeRate());

			//Start playing at codec rate
			audioOutput->StartPlaying(rate, 1);
		}

		//Lo decodificamos
		int len = codec->Decode(packet->GetMediaData(),packet->GetMediaLength(),playBuffer,playBufferSize);

		//Check len
		if (len>0)
		{
			//Obtenemos el tiempo del frame
			frameTime = packet->GetExtTimestamp() - lastTime;

			//Actualizamos el ultimo envio
			lastTime = packet->GetExtTimestamp();

			//Check muted
			if (!muted)
				//Y lo reproducimos
				audioOutput->PlayBuffer(playBuffer,len,frameTime, packet->HasAudioLevel() && packet->GetVAD() ? packet->GetLevel() : -1);
		}


		//Aumentamos el numero de bytes recividos
		recBytes+=packet->GetMediaLength();

	}

	//Check not null
	if (audioOutput)
		//Terminamos de reproducir
		audioOutput->StopPlaying();

	//Check not null
	if (codec)
		//Delete codec
		delete(codec);

	//Exit
	Log("<RecAudio\n");

	//Done
	return 1;
}

/*******************************************
* SendAudio
*	Capturamos el audio y lo mandamos
*******************************************/
int AudioStream::SendAudio()
{
	SWORD 		recBuffer[1024];
	int 		sendBytes=0;
	struct timeval 	before;

	//Log
	Log(">SendAudio\n");

	//Check input
	if (!audioInput)
		//Error
		return Error("-SendAudio failed, audioInput is null");

	//Obtenemos el tiempo ahora
	gettimeofday(&before,NULL);

	//Create audio encoder
	AudioEncoder* codec = AudioCodecFactory::CreateEncoder(audioCodec,audioProperties);
	
	//Check it
	if (!codec)
		//Error
		return Error("-SendAudio failed, could not create audio codec [codec:%d]\n",audioCodec);

	//Get native rate
	DWORD nativeRate = audioInput->GetNativeRate();

	//Get codec rate
	DWORD rate = codec->TrySetRate(nativeRate, 1);

	//Start recording at codec rate
	audioInput->StartRecording(rate);

	//Get clock rate for codec
	DWORD clock = codec->GetClockRate();

	

	//Get initial time
	QWORD frameTime = getDifTime(&ini)*clock/1E6;

	//Send audio
	while(sendingAudio)
	{
		//Create packet
		RTPPacket::shared packet = std::make_shared<RTPPacket>(MediaFrame::Audio,audioCodec);
		
		//Set clock rate
		packet->SetClockRate(clock);
			
		//Increment rtp timestamp
		frameTime += codec->numFrameSamples*clock/rate;

		//Capture audio data
		if (audioInput->RecBuffer(recBuffer,codec->numFrameSamples)==0)
			continue;

		//Encode it
		int len = codec->Encode(recBuffer,codec->numFrameSamples,packet->AdquireMediaData(),packet->GetMaxMediaLength());

		//check result
		if(len<=0)
			continue;

		//Set lengths
		packet->SetMediaLength(len);

		//Set frametime
		packet->SetExtTimestamp(frameTime);

		//Send it
		rtp.SendPacket(packet,frameTime);
	}

	Log("-SendAudio cleanup[%d]\n",sendingAudio);

	//Paramos de grabar por si acaso
	audioInput->StopRecording();

	//Logeamos
	Log("-Deleting codec\n");

	//Borramos el codec
	delete codec;

	//Salimos
        Log("<SendAudio\n");
}

MediaStatistics AudioStream::GetStatistics()
{
	MediaStatistics stats;

	//Fill stats
	stats.isReceiving	= IsReceiving();
	stats.isSending		= IsSending();
	stats.lostRecvPackets   = rtp.GetLostRecvPackets();
	stats.numRecvPackets	= rtp.GetNumRecvPackets();
	stats.numSendPackets	= rtp.GetNumSendPackets();
	stats.totalRecvBytes	= rtp.GetTotalRecvBytes();
	stats.totalSendBytes	= rtp.GetTotalSendBytes();

	//Return it
	return stats;
}

int AudioStream::SetMute(bool isMuted)
{
	//Set it
	muted = isMuted;
	//Exit
	return 1;
}
