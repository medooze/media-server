#include "audiodecoder.h"
#include "media.h"
#include "aac/aacdecoder.h"
#include "AudioCodecFactory.h"

AudioDecoderWorker::~AudioDecoderWorker()
{
	Stop();
}

int AudioDecoderWorker::Start()
{
	Log("-AudioDecoderWorker::Start()\n");

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
	if (!decoding)
		return 0;
	
	Log(">AudioDecoderWorker::Stop()\n");

	//Stop
	decoding=0;

	//Cancel any pending wait
	packets.Cancel();

	//Esperamos
	pthread_join(thread,NULL);

	Log("<AudioDecoderWorker::Stop()\n");

	return 1;
}

void AudioDecoderWorker::AddAudioOuput(AudioOutput* output)
{
	//Ensure we have a valid value
	if (!output)
		//Done
		return;
		
	ScopedLock scope(mutex);
	//Add it
	outputs.insert(output);
	//If we hace started
	if (decoding)
		//Start it
		output->StartPlaying(rate);
}

void AudioDecoderWorker::RemoveAudioOutput(AudioOutput* output)
{
	ScopedLock scope(mutex);
	//Remove from ouput
	if (outputs.erase(output))
		//Stop it
		output->StopPlaying();
}

void AudioDecoderWorker::SetAACConfig(const uint8_t* data,const size_t size)
{
	//IF no codec
	if (!codec)
	{
		//Create new AAC odec from pacekt
		codec.reset(AudioCodecFactory::CreateDecoder(AudioCodec::AAC));

		//Check we found one
		if (!codec)
			//Skip
			return;
		
		//Convert it to AAC encoder
		auto aac = static_cast<AACDecoder*>(codec.get());
		//Set config there
		aac->SetConfig(data,size);

		//Update rate
		rate = codec->GetRate();

		//SYNC block
		{
			ScopedLock scope(mutex);
			//For each output
			for (auto output : outputs)
				//Start playing again
				output->StartPlaying(rate);	
		}
	} else {
		//Convert it to AAC encoder
		auto aac = static_cast<AACDecoder*>(codec.get());
		//Set config there
		aac->SetConfig(data,size);
		
		//SYNC block
		{
			ScopedLock scope(mutex);
			//For each output
			for (auto output : outputs)
			{
				//Stop it
				output->StopPlaying();
				//Start playing again
				output->StartPlaying(rate);	
			}
		}
	}
}


int AudioDecoderWorker::Decode()
{
	SWORD		raw[2048];
	DWORD		rawSize=2048;
	DWORD		frameTime=0;
	DWORD		lastTime=0;

	Log(">AudioDecoderWorker::Decode()\n");

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Obtenemos el paquete
		if (!packets.Wait(0))
		{
			//If no codec
			if (!codec)
			{
				//Stop playing
				ScopedLock scope(mutex);
				//For each output
				for (auto output : outputs)
					//Stop it
					output->StopPlaying();
			}
			//Done
			break;
		}

		//Get packet in queue
		auto packet = packets.Pop();

		//Check
		if (!packet)
			//Check condition again
			continue;

		//If we don't have codec
		if (!codec || (packet->GetCodec()!=codec->type))
		{
			//If no codec
			if (!codec)
			{
				//Stop playing
				ScopedLock scope(mutex);
				//For each output
				for (auto output : outputs)
					//Stop it
					output->StopPlaying();
			}
			
			//Create new codec from pacekt
			codec.reset(AudioCodecFactory::CreateDecoder((AudioCodec::Type)packet->GetCodec()));
				
			//Check we found one
			if (!codec)
				//Skip
				continue;
			
			//Update rate
			rate = codec->GetRate();
			
			//SYNC block
			{
				ScopedLock scope(mutex);
				//For each output
				for (auto output : outputs)
					//Start playing again
					output->StartPlaying(rate);	
			}
			
		}

		//Lo decodificamos
		int len = codec->Decode(packet->GetMediaData(),packet->GetMediaLength(),raw,rawSize);

		//Obtenemos el tiempo del frame
		frameTime = packet->GetTimestamp() - lastTime;

		//Actualizamos el ultimo envio
		lastTime = packet->GetTimestamp();

		//Sync
		{
			ScopedLock scope(mutex);
			//For each output
			for (auto output : outputs)
				//Send buffer
				output->PlayBuffer(raw,len,frameTime);
		}
	}

	//Check codec
	if (codec!=NULL)
	{
		//Stop playing
		ScopedLock scope(mutex);
		//For each output
		for (auto output : outputs)
			//Stop it
			output->StopPlaying();
	}


	Log("<AudioDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void AudioDecoderWorker::onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Put it on the queue
	packets.Add(packet->Clone());
}

void AudioDecoderWorker::onEnded(RTPIncomingMediaStream* stream)
{
	//Cancel packets wait
	packets.Cancel();
}
