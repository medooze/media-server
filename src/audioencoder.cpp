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



/**********************************
* AudioEncoder
*	Constructor
***********************************/
AudioEncoder::AudioEncoder()
{
	//Not encoding
	encodingAudio=0;
	//Set default codec to PCMU
	audioCodec=AudioCodec::PCMU;
	//Create mutex
	pthread_mutex_init(&mutex,0);
}

/*******************************
* ~AudioEncoder
*	Destructor.
********************************/
AudioEncoder::~AudioEncoder()
{
	//If still running
	if (encodingAudio)
		//End
		End();
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

/***************************************
* CreateAudioCodec
* 	Crea un objeto de codec de audio del tipo correspondiente
****************************************/
AudioCodec* AudioEncoder::CreateAudioCodec(AudioCodec::Type codec)
{

	Log("-CreateAudioCodec [%d]\n",codec);

	//Creamos uno dependiendo del tipo
	switch(codec)
	{
		case AudioCodec::GSM:
			return new GSMCodec();
		case AudioCodec::PCMA:
			return new PCMACodec();
		case AudioCodec::PCMU:
			return new PCMUCodec();
		default:
			Log("Codec de audio erroneo [%d]\n",codec);
	}

	return NULL;
}

/***************************************
* SetAudioCodec
*	Fija el codec de audio
***************************************/
int AudioEncoder::SetAudioCodec(AudioCodec::Type codec)
{
	//Compromabos que soportamos el modo
	if (!(codec==AudioCodec::PCMA || codec==AudioCodec::GSM || codec==AudioCodec::PCMU))
		return 0;

	//Colocamos el tipo de audio
	audioCodec = codec;

	Log("-SetAudioCodec [%d,%s]\n",audioCodec,AudioCodec::GetNameFor(audioCodec));

	//Y salimos
	return 1;
}

/***************************************
* Init
*	Inicializa los devices
***************************************/
int AudioEncoder::Init(AudioInput *input)
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
void * AudioEncoder::startEncoding(void *par)
{
	AudioEncoder *conf = (AudioEncoder *)par;
	blocksignals();
	Log("Encoding audio [%d]\n",getpid());
	pthread_exit((void *)conf->Encode());
}


/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int AudioEncoder::StartEncoding()
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
int AudioEncoder::End()
{
	//Terminamos de enviar
	StopEncoding();

	return 1;
}


/***************************************
* StopEncoding
* 	Termina el envio
****************************************/
int AudioEncoder::StopEncoding()
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
int AudioEncoder::Encode()
{
	WORD 		recBuffer[512];
        struct timeval 	before;
	AudioCodec* 	codec;
	DWORD		frameTime=0;

	Log(">Encode Audio\n");

	//Obtenemos el tiempo ahora
	gettimeofday(&before,NULL);

	//Creamos el codec de audio
	if ((codec = CreateAudioCodec(audioCodec))==NULL)
	{
		Log("Error en el envio de audio,saliendo\n");
		return 0;
	}

	//Create audio frame
	AudioFrame frame(audioCodec);

	//Empezamos a grabar
	audioInput->StartRecording();

	//Mientras tengamos que capturar
	while(encodingAudio)
	{
		//Capturamos 20ms
		if (audioInput->RecBuffer((WORD *)recBuffer,160)==0)
			//Skip and probably exit
			continue;

		//Incrementamos el tiempo de envio
		frameTime += 160;

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

			//Set frame time
			frame.SetTimestamp(frameTime);

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
	
	pthread_exit(0);
}

bool AudioEncoder::AddListener(MediaFrame::Listener *listener)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Add to set
	listeners.insert(listener);

	//unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

bool AudioEncoder::RemoveListener(MediaFrame::Listener *listener)
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
