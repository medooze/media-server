/* 
 * File:   AudioEncoderWorker.cpp
 * Author: Sergio
 * 
 * Created on 4 de octubre de 2011, 20:42
 */

#include <set>
#include "log.h"
#include "AudioEncoderWorker.h"

AudioEncoderWorker::AudioEncoderWorker()
{
	//Nothing
	input = NULL;
	encoding = false;
	codec = (AudioCodec::Type)-1;
}

AudioEncoderWorker::~AudioEncoderWorker()
{
	End();
}

int AudioEncoderWorker::Init(AudioInput *input)
{
	//Store it
	this->input = input;
}
int AudioEncoderWorker::SetCodec(AudioCodec::Type codec)
{
	//Colocamos el tipo de audio
	this->codec = codec;

	//Check
	if (!listeners.empty() && !encoding)
		//Start
		Start();

	return 1;
}

int AudioEncoderWorker::Start()
{
	//Check
	if (!input)
		//Exit
		return Error("null audio input");

	//Check if need to restart
	if (encoding)
		//Stop first
		Stop();

	//Start decoding
	encoding = 1;

	//launc thread
	createPriorityThread(&thread,startEncoding,this,0);

	return 1;
}
void * AudioEncoderWorker::startEncoding(void *par)
{
	Log("AudioEncoderWorkerThread [%d]\n",getpid());
	//Get worker
	AudioEncoderWorker *worker = (AudioEncoderWorker *)par;
	//Block all signals
	blocksignals();
	//Run
	pthread_exit((void *)worker->Encode());
}

int AudioEncoderWorker::Stop()
{
	Log(">Stop AudioEncoderWorker\n");

	//If we were started
	if (encoding)
	{
		//Stop
		encoding=0;

		//Esperamos
		pthread_join(thread,NULL);
	}

	Log("<Stop AudioEncoderWorker\n");

	return 1;
}

int AudioEncoderWorker::End()
{
	//Check if already decoding
	if (encoding)
		//Stop
		Stop();

	//Set null
	input = NULL;
}


/*******************************************
* SendAudio
*	Capturamos el audio y lo mandamos
*******************************************/
int AudioEncoderWorker::Encode()
{
	RTPPacket	packet(MediaFrame::Audio,codec,codec);
	SWORD 		recBuffer[512];
	AudioCodec* 	encoder;
	DWORD		frameTime=0;

	Log(">Encode AudioEncoderWorker [%d,%s]\n",codec,AudioCodec::GetNameFor(codec));

	//Create the audio codec
	if ((encoder = AudioCodec::CreateCodec(codec))==NULL)
	{
		//Not encoding
		encoding = false;
		//Exit
		return Error("Could not create codec\n");
	}

	//Empezamos a grabar
	input->StartRecording();

	//Mientras tengamos que capturar
	while(encoding)
	{
		//Incrementamos el tiempo de envio
		frameTime += encoder->numFrameSamples;

		//Capturamos
		if (input->RecBuffer(recBuffer,encoder->numFrameSamples)==0)
			continue;

		//Lo codificamos
		int len = encoder->Encode(recBuffer,encoder->numFrameSamples,packet.GetMediaData(),packet.GetMaxMediaLength());

		//Comprobamos que ha sido correcto
		if(len<=0)
			continue;

		//Set frame time
		packet.SetTimestamp(frameTime);
		//Set length
		packet.SetMediaLength(len);
		
		//Multiplex it
		Multiplex(packet);
	}

	Log("-SendAudio cleanup[%d]\n",encoding);

	//Paramos de grabar por si acaso
	input->StopRecording();

	//Logeamos
	Log("-Deleting codec\n");

	//Borramos el codec
	delete encoder;

	//Salimos
        Log("<SendAudio\n");
	
	pthread_exit(0);
}

void AudioEncoderWorker::AddListener(Listener *listener)
{
	//Check if we were already encoding
	if (listener && !encoding && codec!=-1)
		//Start encoding;
		Start();
	//Add the listener
	RTPMultiplexer::AddListener(listener);
}

void AudioEncoderWorker::RemoveListener(Listener *listener)
{
	//Remove the listener
	RTPMultiplexer::RemoveListener(listener);
	//If there are no more
	if (listeners.empty())
		//Stop encoding
		Stop();
}

void AudioEncoderWorker::Update()
{
}
