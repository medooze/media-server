#include "AudioDecoderWorker.h"
#include "media.h"
#include "aac/AACDecoder.h"
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
	frames.Cancel();

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
	if (!audioDecoder)
	{
		//Create new AAC odec from pacekt
		audioDecoder.reset(AudioCodecFactory::CreateDecoder(AudioCodec::AAC));

		//Check we found one
		if (!audioDecoder)
			//Skip
			return;
	} else {
		//For each output
		for (auto output : outputs)
			//Stop it
			output->StopPlaying();
	}
	
	//Convert it to AAC encoder
	auto aac = static_cast<AACDecoder*>(audioDecoder.get());
	//Set config there
	aac->SetConfig(data,size);

	//Update rate
	rate = audioDecoder->GetRate();
	numChannels = audioDecoder->GetNumChannels();

	//For each output
	for (auto output : outputs)
		//Start playing again
		output->StartPlaying(rate, numChannels);
}


int AudioDecoderWorker::Decode()
{
	Log(">AudioDecoderWorker::Decode()\n");

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Obtenemos el paquete
		if (!frames.Wait(0))
			//Done
			break;

		//Get packet in queue
		auto frame = frames.Pop();

		//Check
		if (!frame)
			//Check condition again
			continue;
		//SYNC
		{
			//Lock
			ScopedLock scope(mutex);

			//If we don't have codec
			if (!audioDecoder || (frame->GetCodec()!=audioDecoder->type))
			{
				//If got a previous codec
				if (audioDecoder)
					//For each output
					for (auto output : outputs)
						//Stop it
						output->StopPlaying();

				//Create new codec from pacekt
				audioDecoder.reset(AudioCodecFactory::CreateDecoder((AudioCodec::Type)frame->GetCodec()));

				//Check we found one
				if (!audioDecoder)
					//Skip
					continue;

				//Update rate
				rate = audioDecoder->GetRate();
				numChannels = audioDecoder->GetNumChannels();

				//Ensure that we have rate and samples
				if (!rate || !numChannels)
					//skip
					continue;

				//For each output
				for (auto output : outputs)
					//Start playing again
					output->StartPlaying(rate, numChannels);
			}

			if(!audioDecoder->Decode(frame))
				continue;
			auto audioFramePTS = frame->GetTimestamp();
			while (auto audioBuffer = audioDecoder->GetDecodedAudioFrame())
			{
				//Check if we have a different channel count
				if (numChannels != audioDecoder->GetNumChannels())
				{
					//Update rate
					rate = audioDecoder->GetRate();
					numChannels = audioDecoder->GetNumChannels();

					//For each output
					for (auto output : outputs)
					{
						//Stop it
						output->StopPlaying();
						//Start playing again
						output->StartPlaying(rate, numChannels);
					}
				}

				audioBuffer->SetTimestamp(audioFramePTS);
				audioFramePTS += audioBuffer->GetNumSamples()/audioBuffer->GetNumChannels();
				audioBuffer->SetClockRate(frame->GetClockRate());
				//For each output
				for (auto output : outputs)
					//Send buffer
					output->PlayBuffer(audioBuffer);
			}	
		}
	}

	//SYNC
	{
		//Stop playing
		ScopedLock scope(mutex);
		//Check codec
		if (audioDecoder)
			//For each output
			for (auto output : outputs)
				//Stop it
				output->StopPlaying();
	}


	Log("<AudioDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void AudioDecoderWorker::onMediaFrame(const MediaFrame& frame)
{
	//Ensure it is audio
	if (frame.GetType()!=MediaFrame::Audio)
	{
		Warning("-AudioDecoderWorker::onMediaFrame()  got wrong frame type: %s [this: %p]\n", MediaFrame::TypeToString(frame.GetType()), this);
		return;
	}

	//Put it on the queue
	frames.Add(std::shared_ptr<AudioFrame>(static_cast<AudioFrame*>(frame.Clone())));
}