#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <set>
#include "log.h"
#include "tools.h"
#include "audio.h"
#include "audioencoder.h"
#include "AudioCodecFactory.h"
#include "opus/opusconfig.h"


/**********************************
* AudioEncoderWorker
*	Constructor
***********************************/
AudioEncoderWorker::AudioEncoderWorker()
{
	Log("-AudioEncoderWorker::AudioEncoderWorker()\n");

	//Create mutex
	pthread_mutex_init(&mutex,0);
}

/*******************************
* ~AudioEncoderWorker
*	Destructor.
********************************/
AudioEncoderWorker::~AudioEncoderWorker()
{
	Log("-AudioEncoderWorker::~AudioEncoderWorker()\n");

	//If still running
	if (encodingAudio)
		//End
		End();
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

/***************************************
* SetAudioCodec
*	Fija el codec de audio
***************************************/
int AudioEncoderWorker::SetAudioCodec(AudioCodec::Type codec, const Properties& properties)
{
	//Colocamos el tipo de audio
	audioCodec = codec;
	audioProperties = properties;

	Log("-AudioEncoderWorker::SetAudioCodec() [%d,%s]\n",audioCodec,AudioCodec::GetNameFor(audioCodec));

	//Y salimos
	return 1;
}

/***************************************
* Init
*	Inicializa los devices
***************************************/
int AudioEncoderWorker::Init(AudioInput *input)
{
	Log(">AudioEncoderWorker::Init()\n");

	//Nos quedamos con los puntericos
	audioInput  = input;

	//Y aun no estamos mandando nada
	encodingAudio=0;

	Log("<AudioEncoderWorker::Init()n");

	return 1;
}

/***************************************
* startencodingAudio
*	Helper function
***************************************/
void * AudioEncoderWorker::startEncoding(void *par)
{
	AudioEncoderWorker *conf = (AudioEncoderWorker *)par;
	blocksignals();
	conf->Encode();
	//Exit
	return NULL;
}


/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int AudioEncoderWorker::StartEncoding()
{
	//Si estabamos mandando tenemos que parar
	if (encodingAudio)
		//paramos
		StopEncoding();

	encodingAudio=1;

	//Start thread
	createPriorityThread(&encodingAudioThread,startEncoding,this,1);

	return 1;
}
/***************************************
* End
*	Termina la conferencia activa
***************************************/
int AudioEncoderWorker::End()
{
	Log("-AudioEncoderWorker::End()\n");

	//Terminamos de enviar
	StopEncoding();

	return 1;
}


/***************************************
* StopEncoding
* 	Termina el envio
****************************************/
int AudioEncoderWorker::StopEncoding()
{
	Log(">AudioEncoderWorker::StopEncoding()\n");

	//Esperamos a que se cierren las threads de envio
	if (encodingAudio)
	{
		//paramos
		encodingAudio=0;

		//Cancel any pending audio
		audioInput->CancelRecBuffer();

		//Y esperamos
		pthread_join(encodingAudioThread,NULL);
	}

	Log("<AudioEncoderWorker::StopEncoding()\n");

	return 1;
}



/*******************************************
* Encode
*	Capturamos el audio y lo mandamos
*******************************************/
int AudioEncoderWorker::Encode()
{
	SWORD 		recBuffer[2048];
	AudioEncoder* 	codec;
	QWORD		frameTime=0;

	Log(">AudioEncoderWorker::Encode()\n");

	//Creamos el codec de audio
	if ((codec = AudioCodecFactory::CreateEncoder(audioCodec,audioProperties))==NULL)
		return Error("-AudioEncoderWorker::Encode() | Could not open encoder");

	//Try to set native rate
	DWORD numChannels = audioInput->GetNumChannels();
	DWORD rate = audioInput->GetNativeRate();
	
	//Update codec
	rate = codec->TrySetRate(rate, numChannels);
	
	//Create audio frame
	AudioFrame frame(audioCodec);

	//If it is opus
	if (audioCodec == AudioCodec::OPUS)
	{
		//Create opus config
		OpusConfig config(numChannels, rate);

		//Serialize config and add it to frame
		frame.AllocateCodecConfig(config.GetSize());
		config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
	}
	
	//Disable shared buffer on clone
	frame.DisableSharedBuffer();

	//Set rate
	frame.SetClockRate(rate);

	//Empezamos a grabar
	audioInput->StartRecording(rate);

	//Mientras tengamos que capturar
	while(encodingAudio)
	{
		//Capturamos 20ms
		if (audioInput->RecBuffer(recBuffer,codec->numFrameSamples)==0)
			//Skip and probably exit
			continue;

		//Incrementamos el tiempo de envio
		frameTime += codec->numFrameSamples;

		//If we have a different channel count
		if (numChannels != audioInput->GetNumChannels())
		{
			//Update channel count
			numChannels = audioInput->GetNumChannels();
			//Set new channel count on codec
			codec->TrySetRate(rate, numChannels);

			//If it is opus
			if (audioCodec == AudioCodec::OPUS)
			{
				//Create opus config
				OpusConfig config(numChannels,rate);

				//Serialize config and add it to frame
				frame.AllocateCodecConfig(config.GetSize());
				config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
			}
		}

		//Lo codificamos
		int len = codec->Encode(recBuffer,codec->numFrameSamples,frame.GetData(),frame.GetMaxMediaLength());

		//Comprobamos que ha sido correcto
		if(len<=0)
		{
			Log("-AudioEncoderWorker::Encode() | Error encoding audio\n");
			continue;
		}

		//Set frame length
		frame.SetLength(len);
		
		//Set frame timestamp
		frame.SetTimestamp(frameTime);
		//Set frame timestamp
		frame.SetSenderTime(frameTime * 1000 / codec->GetClockRate());
		//Set encoded time
		frame.SetTime(getTime()/1000);
		//Set frame duration
		frame.SetDuration(codec->numFrameSamples);
		//Set number of channels
		frame.SetNumChannels(numChannels);

		//Clear rtp
		frame.ClearRTPPacketizationInfo();
			
		//Add rtp packet
		frame.AddRtpPacket(0,len,NULL,0);
		 
		//Lock
		pthread_mutex_lock(&mutex);

		//For each listener
		for (auto& listener : listeners)
		{
			//If was not null
			if (listener)
				//Call listener
				listener->onMediaFrame(frame);
		}

		//unlock
		pthread_mutex_unlock(&mutex);
	}

	Debug("-AudioEncoderWorker::Encode() | cleanup[%d]\n",encodingAudio);

	//Paramos de grabar por si acaso
	audioInput->StopRecording();

	
	//Borramos el codec
	delete codec;

	//Salimos
        Log("<AudioEncoderWorker::Encode()\n");
	
	return 1;
}

bool AudioEncoderWorker::AddListener(const MediaFrame::Listener::shared& listener)
{

	Debug("-AudioEncoderWorker::AddListener() | [listener:%p]\n", listener.get());

	//Lock
	pthread_mutex_lock(&mutex);

	//Add to set
	listeners.insert(listener);

	//unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

bool AudioEncoderWorker::RemoveListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-AudioEncoderWorker::RemoveListener() | [listener:%p]\n", listener.get());

	//Lock
	pthread_mutex_lock(&mutex);

	//Erase it
	listeners.erase(listener);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}
