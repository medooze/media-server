#include "audiodecoder.h"
#include "media.h"
#include "aac/aacdecoder.h"
#include "AudioCodecFactory.h"

AudioDecoderWorker::~AudioDecoderWorker()
{
	Log("-AudioDecoderWorker::~AudioDecoderWorker()\n");

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
	if (decoding && rate)
		//Start it
		output->StartPlaying(rate, numChannels);
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
	ScopedLock scope(mutex);
		
	//IF no codec
	if (!codec)
	{
		//Create new AAC odec from pacekt
		codec.reset(AudioCodecFactory::CreateDecoder(AudioCodec::AAC));

		//Check we found one
		if (!codec)
			//Skip
			return;
	} else {
		//For each output
		for (auto output : outputs)
			//Stop it
			output->StopPlaying();
	}
	
	//Convert it to AAC encoder
	auto aac = static_cast<AACDecoder*>(codec.get());
	//Set config there
	aac->SetConfig(data,size);

	//Update rate
	rate = codec->GetRate();
	numChannels = codec->GetNumChannels();

	//For each output
	for (auto output : outputs)
		//Start playing again
		output->StartPlaying(rate, numChannels);
}


int AudioDecoderWorker::Decode()
{
	SWORD		raw[4096];
	DWORD		rawSize=4096;
	QWORD		frameTime=0;
	QWORD		lastTime=0;

	Log(">AudioDecoderWorker::Decode()\n");

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Obtenemos el paquete
		if (!packets.Wait(0))
			//Done
			break;

		//Get packet in queue
		auto packet = packets.Pop();

		//Check
		if (!packet)
			//Check condition again
			continue;
		
		//SYNC
		{
			//Lock
			ScopedLock scope(mutex);

			//If we don't have codec
			if (!codec || (packet->GetCodec()!=codec->type))
			{
				//If got a previous codec
				if (codec)
					//For each output
					for (auto output : outputs)
						//Stop it
						output->StopPlaying();

				//Create new codec from pacekt
				codec.reset(AudioCodecFactory::CreateDecoder((AudioCodec::Type)packet->GetCodec()));

				//Check we found one
				if (!codec)
					//Skip
					continue;

				//If it is aac and we have config
				if (codec->type==AudioCodec::AAC && packet->config && !packet->config->IsEmpty())
				{
					//Convert it to AAC encoder
					auto aac = static_cast<AACDecoder*>(codec.get());
					//Set config there
					aac->SetConfig(packet->config->GetData(), packet->config->GetSize());
				}

				//Update rate
				rate = codec->GetRate();
				numChannels = codec->GetNumChannels();

				//Ensure that we have rate and samples
				if (!rate || !numChannels)
					//skip
					continue;

				//For each output
				for (auto output : outputs)
					//Start playing again
					output->StartPlaying(rate, numChannels);
			}

			//Lo decodificamos
			int len = codec->Decode(packet->GetMediaData(),packet->GetMediaLength(),raw,rawSize);
			
			//Check if we have a different channel count
			if (numChannels != codec->GetNumChannels())
			{
				//Update rate
				rate = codec->GetRate();
				numChannels = codec->GetNumChannels();

				//For each output
				for (auto output : outputs)
				{
					//Stop it
					output->StopPlaying();
					//Start playing again
					output->StartPlaying(rate, numChannels);
				}
			}

			//Get last frame time duration
			frameTime = packet->GetExtTimestamp() - lastTime;

			//Update last sent time
			lastTime = packet->GetExtTimestamp();

			//For each output
			for (auto output : outputs)
				//Send buffer
				output->PlayBuffer(raw, len, frameTime);
		}
	}

	//SYNC
	{
		//Stop playing
		ScopedLock scope(mutex);
		//Check codec
		if (codec)
			//For each output
			for (auto output : outputs)
				//Stop it
				output->StopPlaying();
	}


	Log("<AudioDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void AudioDecoderWorker::onRTP(const RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Put it on the queue
	packets.Add(packet->Clone());
}

void AudioDecoderWorker::onEnded(const RTPIncomingMediaStream* stream)
{
	//Cancel packets wait
	packets.Cancel();
}


void AudioDecoderWorker::onBye(const RTPIncomingMediaStream* stream)
{
	//Cancel packets wait
	packets.Cancel();
}
