#include "audiodecoder.h"
#include "media.h"

AudioDecoderWorker::AudioDecoderWorker()
{
	//Nothing
	output = NULL;
	decoding = false;
}

AudioDecoderWorker::~AudioDecoderWorker()
{
	End();
}

int AudioDecoderWorker::Init(AudioOutput *output)
{
	//Store it
	this->output = output;
}

int AudioDecoderWorker::End()
{
	//Check if already decoding
	if (decoding)
		//Stop
		Stop();
}

int AudioDecoderWorker::Start()
{
	Log("-StartAudioDecoder\n");

	//Check
	if (!output)
		//Exit
		return Error("null audio output");

	//Check if need to restart
	if (decoding)
		//Stop first
		Stop();

	//Start decoding
	decoding = 1;

	//launc thread
	createPriorityThread(&thread,startDecoding,this,0);

	return 1;
}
void * AudioDecoderWorker::startDecoding(void *par)
{
	Log("AudioDecoderThread [%p]\n",pthread_self());
	//Get worker
	AudioDecoderWorker *worker = (AudioDecoderWorker *)par;
	//Block all signals
	blocksignals();
	//Run
	worker->Decode();
	//Exit
	return NULL;;
}

int  AudioDecoderWorker::Stop()
{
	Log(">StopAudioDecoder\n");

	//If we were started
	if (decoding)
	{
		//Stop
		decoding=0;

		//Cancel any pending wait
		packets.Cancel();

		//Esperamos
		pthread_join(thread,NULL);
	}

	Log("<StopAudioDecoder\n");

	return 1;
}


int AudioDecoderWorker::Decode()
{
	SWORD		raw[512];
	DWORD		rawSize=512;
	AudioDecoder*	codec=NULL;
	DWORD		frameTime=0;
	DWORD		lastTime=0;

	Log(">DecodeAudio\n");

	//Empezamos a reproducir
	output->StartPlaying(8000);

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Obtenemos el paquete
		if (!packets.Wait(0))
			//Check condition again
			continue;

		//Get packet in queue
		RTPPacket* packet = packets.Pop();

		//Check
		if (!packet)
			//Check condition again
			continue;

		//Comprobamos el tipo
		if ((codec==NULL) || (packet->GetCodec()!=codec->type))
		{
			//Si habia uno nos lo cargamos
			if (codec!=NULL)
				delete codec;

			//Creamos uno dependiendo del tipo
			if (!(codec = AudioCodecFactory::CreateDecoder((AudioCodec::Type)packet->GetCodec())))
				continue;

		}

		//Lo decodificamos
		int len = codec->Decode(packet->GetMediaData(),packet->GetMediaLength(),raw,rawSize);

		//Obtenemos el tiempo del frame
		frameTime = packet->GetTimestamp() - lastTime;

		//Actualizamos el ultimo envio
		lastTime = packet->GetTimestamp();

		//Y lo reproducimos
		output->PlayBuffer(raw,len,frameTime);

		//Delete packet
		delete(packet);
	}

	//End reproducing
	output->StopPlaying();

	//Check codec
	if (codec!=NULL)
		//Delete object
		delete codec;


	Log("<DecodeAudio\n");

	//Exit
	return 0;
}

void AudioDecoderWorker::onRTPPacket(RTPPacket &packet)
{
	//Put it on the queue
	packets.Add(packet.Clone());
}
