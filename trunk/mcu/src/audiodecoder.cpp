#include "audiodecoder.h"
#include "media.h"

AudioDecoder::AudioDecoder()
{
	//Nothing
	output = NULL;
	decoding = false;
}

AudioDecoder::~AudioDecoder()
{
	End();
}

int AudioDecoder::Init(AudioOutput *output)
{
	//Store it
	this->output = output;
}

int AudioDecoder::End()
{
	//Check if already decoding
	if (decoding)
		//Stop
		Stop();
}

int AudioDecoder::Start()
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
void * AudioDecoder::startDecoding(void *par)
{
	Log("AudioDecoderThread [%d]\n",getpid());
	//Get worker
	AudioDecoder *worker = (AudioDecoder *)par;
	//Block all signals
	blocksignals();
	//Run
	pthread_exit((void *)worker->Decode());
}

int  AudioDecoder::Stop()
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


int AudioDecoder::Decode()
{
	WORD		raw[512];
	DWORD		rawSize=512;
	AudioCodec*	codec=NULL;
	DWORD		frameTime=0;
	DWORD		lastTime=0;

	Log(">DecodeAudio\n");

	//Empezamos a reproducir
	output->StartPlaying();

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
			if (!(codec = AudioCodec::CreateCodec((AudioCodec::Type)packet->GetCodec())))
				continue;

		}

		//Lo decodificamos
		int len = codec->Decode(packet->GetMediaData(),packet->GetMediaLength(),raw,rawSize);

		//Obtenemos el tiempo del frame
		frameTime = packet->GetTimestamp() - lastTime;

		//Actualizamos el ultimo envio
		lastTime = packet->GetTimestamp();

		//Y lo reproducimos
		output->PlayBuffer((WORD *)raw,len,frameTime);

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
	pthread_exit(0);
}

void AudioDecoder::onRTPPacket(RTPPacket &packet)
{
	//Put it on the queue
	packets.Add(packet.Clone());
}
