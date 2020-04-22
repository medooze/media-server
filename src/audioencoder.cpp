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
#include "g711/g711codec.h"
#include "gsm/gsmcodec.h"
#include "audioencoder.h"
#include "AudioCodecFactory.h"


/**********************************
* AudioEncoderWorker
*	Constructor
***********************************/
AudioEncoderWorker::AudioEncoderWorker()
{
	//Not encoding
	encodingAudio=0;
	//Set default codec to PCMU
	audioCodec=AudioCodec::PCMU;
	//Create mutex
	pthread_mutex_init(&mutex,0);
}

/*******************************
* ~AudioEncoderWorker
*	Destructor.
********************************/
AudioEncoderWorker::~AudioEncoderWorker()
{
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

	Log("-SetAudioCodec [%d,%s]\n",audioCodec,AudioCodec::GetNameFor(audioCodec));

	//Y salimos
	return 1;
}

/***************************************
* Init
*	Inicializa los devices
***************************************/
int AudioEncoderWorker::Init(AudioInput *input)
{
	Log(">Init audio encoder\n");

	//Nos quedamos con los puntericos
	audioInput  = input;

	//Y aun no estamos mandando nada
	encodingAudio=0;

	Log("<Init audio encoder\n");

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
	Log("Encoding audio [%p]\n",pthread_self());
	conf->Encode();
	//Exit
	return NULL;;
}


/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int AudioEncoderWorker::StartEncoding()
{
	Log(">Start encoding audio\n");

	//Si estabamos mandando tenemos que parar
	if (encodingAudio)
		//paramos
		StopEncoding();

	encodingAudio=1;

	//Start thread
	createPriorityThread(&encodingAudioThread,startEncoding,this,1);

	Log("<StartSending audio [%d]\n",encodingAudio);

	return 1;
}
/***************************************
* End
*	Termina la conferencia activa
***************************************/
int AudioEncoderWorker::End()
{
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
	Log(">StopEncoding Audio\n");

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

	Log("<StopEncoding Audio\n");

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

	Log(">Encode Audio\n");

	//Creamos el codec de audio
	if ((codec = AudioCodecFactory::CreateEncoder(audioCodec,audioProperties))==NULL)
		return Error("Could not open encoder");

	//Try to set native rate
	DWORD rate = codec->TrySetRate(audioInput->GetNativeRate());
	
	//Create audio frame
	AudioFrame frame(audioCodec);
	
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

		//Check codec
		if (codec)
		{
			//Lo codificamos
			int len = codec->Encode(recBuffer,codec->numFrameSamples,frame.GetData(),frame.GetMaxMediaLength());

			//Comprobamos que ha sido correcto
			if(len<=0)
			{
				Log("Error codificando el packete de audio\n");
				continue;
			}

			//Set frame length
			frame.SetLength(len);

			//Set frame timestamp
			frame.SetTimestamp(frameTime);
			//Set time
			frame.SetTime(frameTime*1000/codec->GetClockRate());
			//Set frame duration
			frame.SetDuration(codec->numFrameSamples);

			//Clear rtp
			frame.ClearRTPPacketizationInfo();
			
			//Add rtp packet
			frame.AddRtpPacket(0,len,NULL,0);
		 
			//Lock
			pthread_mutex_lock(&mutex);

			//For each listener
			for (Listeners::iterator it=listeners.begin(); it!=listeners.end(); ++it)
			{
				//Get listener
				MediaFrame::Listener* listener =  *it;
				//If was not null
				if (listener)
					//Call listener
					listener->onMediaFrame(frame);
			}

			//unlock
			pthread_mutex_unlock(&mutex);
		}
		
	}

	Log("-Encode Audio cleanup[%d]\n",encodingAudio);

	//Paramos de grabar por si acaso
	audioInput->StopRecording();

	//Logeamos
	Log("-Deleting codec\n");

	//Borramos el codec
	delete codec;

	//Salimos
        Log("<Encode Audio\n");
	
	return 1;
}

bool AudioEncoderWorker::AddListener(MediaFrame::Listener *listener)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Add to set
	listeners.insert(listener);

	//unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

bool AudioEncoderWorker::RemoveListener(MediaFrame::Listener *listener)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Search
	Listeners::iterator it = listeners.find(listener);

	//If found
	if (it!=listeners.end())
		//Erase it
		listeners.erase(it);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}
