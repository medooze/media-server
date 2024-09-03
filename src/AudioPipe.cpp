#include "log.h"
#include "AudioPipe.h"
#include <math.h>
	
AudioPipe::AudioPipe(DWORD rate)
{
	//Set rate
	nativeRate = rate;
	playRate = rate;
	recordRate = rate;
	
	//Init mutex and cond
	pthread_mutex_init(&mutex,0);
	pthread_cond_init(&cond,0);
}
AudioPipe::~AudioPipe()
{
	CancelRecBuffer();
	StopPlaying();
	StopRecording();
	
	//Free mutex and cond
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}
	
int AudioPipe::StartRecording(DWORD rate)
{
	Log("-AudioPipe start recording [rate:%d]\n",rate);

	//Lock
	pthread_mutex_lock(&mutex);

	//If rate has changed
	if (rate != recordRate)
	{
		// Clear playing buffer
		rateBlockBuffer.clear();
	}
		
	//Store recording rate
	recordRate = rate;
	//If we already had an open transcoder
	if (transrater.IsOpen())
		//Close it
		transrater.Close();

	//if rates are different
	if (playRate != recordRate)
		//Open it
		transrater.Open(playRate, recordRate, numChannels);
	//We are recording now
	recording = true;

	//Signal
	pthread_cond_signal(&cond);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

int AudioPipe::StopRecording()
{
	//If we were recording
	if (!recording)
		//Done
		return false;
	
	Log("-AudioPipe stop recording\n");
	
	//Lock
	pthread_mutex_lock(&mutex);
	
	//Estamos grabando
	recording = false;

	//Signal
	pthread_cond_signal(&cond);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

void  AudioPipe::CancelRecBuffer()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Cancel
	canceled = true;

	//Signal
	pthread_cond_signal(&cond);

	//Unloco mutex
	pthread_mutex_unlock(&mutex);
}

int AudioPipe::StartPlaying(DWORD rate, DWORD numChannels)
{
	Log("-AudioPipe start playing [rate:%d,channels:%d]\n", rate, numChannels);

	//Lock
	pthread_mutex_lock(&mutex);

	//Store play rate
	playRate = rate;
	//And number of channels
	this->numChannels = numChannels;

	//If we already had an open transcoder
	if (transrater.IsOpen())
		//Close it
		transrater.Close();

	//if rates are different
	if (playRate != recordRate)
		//Open it
		transrater.Open(playRate, recordRate, numChannels);
	
	//We are playing
	playing = true;
	
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//Exit
	return true;
}

int AudioPipe::StopPlaying()
{
	//Check if playing already
	if (!playing)
		//DOne
		return false;
	
	Log("-AudioPipe stop playing\n");

	//Lock
	pthread_mutex_lock(&mutex);
	//Close transrater
	transrater.Close();
	//We are not playing
	playing = false;
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//Exit
	return true;
}


int AudioPipe::PlayBuffer(const AudioBuffer::shared& audioBuffer)
{
	//Lock
	pthread_mutex_lock(&mutex);

	auto buffer = const_cast<SWORD*>(audioBuffer->GetData());
	auto totalSamples = audioBuffer->GetNumSamples()*numChannels;

	//Don't do anything if nobody is listening
	if (!recording)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Ok
		return queue.length();
	}
	if(decoderPTSOffset == std::numeric_limits<QWORD>::max()) 
		decoderPTSOffset = audioBuffer->GetTimestamp();
	
	//Check if we are transtrating
	SWORD resampled[8192];
	if (transrater.IsOpen())
	{
		DWORD numResampled = std::round<uint64_t>(audioBuffer->GetNumSamples() * (static_cast<double>(recordRate) / playRate));
		//Proccess
		if (!transrater.ProcessBuffer(buffer, totalSamples, resampled, &numResampled))
		{
			//Error
			pthread_mutex_unlock(&mutex);
			return Error("-AudioPipe::PlayBuffer() could not transrate\n");
		}
		int len = audioBuffer->CopyResampled(resampled, numResampled);
		if(len < numResampled)
		{
			pthread_mutex_unlock(&mutex);
			return Error("-AudioPipe::PlayBuffer() less resampled audio data copied, actual=%d - expected=%d\n", len, numResampled);
		}	
	}
	queue.push_back(audioBuffer);
	availableNumSamples += audioBuffer->GetNumSamples()*numChannels;
	//Signal rec
	pthread_cond_signal(&cond);
	//Unlock
	pthread_mutex_unlock(&mutex);
	return queue.length();
}


AudioBuffer::shared AudioPipe::RecBuffer(DWORD frameSize)
{
	//Lock
	pthread_mutex_lock(&mutex);
	//Ensere we are playing
	while (!playing) 
	{
		//If we have been canceled already
		if (canceled)
		{
			//Remove flag
			canceled = false;
			//Exit
			Log("AudioPipe: RecBuffer cancelled.\n");
			//End
			pthread_mutex_unlock(&mutex);
			return {};
		}
		//Wait for change
		pthread_cond_wait(&cond, &mutex);
	}
	// TODO: use object pool to reuse audiobuffer
	AudioBuffer::shared audioBuffer = std::make_shared<AudioBuffer>(frameSize, numChannels);
	audioBuffer->SetClockRate(recordRate);
	// put it to waiting state if audio pipe has less available samples than what's needed by encoder
	QWORD requiredNumSamples = frameSize * numChannels; 
	while (!canceled && recording && requiredNumSamples > availableNumSamples)
	{
		//Wait for change
		pthread_cond_wait(&cond, &mutex);
		//If we have been canceled
		if (canceled)
		{
			//Remove flag
			canceled = false;
			//Exit
			Log("AudioPipe: RecBuffer cancelled.\n");
			//End
			pthread_mutex_unlock(&mutex);
			return {};
		}
	}

	int readSamples = std::min(rateBlockBuffer.length() / numChannels, frameSize);
	rateBlockBuffer.pop(audioBuffer->ConsumeData(readSamples*numChannels), readSamples*numChannels);
	int needSamples = frameSize - readSamples;

	AudioBuffer::const_shared frontAudioBuffer;
	// if need > 0 which means need to pop audioBuffer queue to get number of required samples
	while(queue.length()>0 && needSamples>0)
	{
		frontAudioBuffer = queue.front();
		auto samples = frontAudioBuffer->GetNumSamples();
		audioBuffer->CopyAudioBufferData(frontAudioBuffer, std::min(needSamples, samples));
		needSamples -= frontAudioBuffer->GetNumSamples();
		queue.pop_front();
	}
	int left = -needSamples;
	if (left > 0)
	{
		int used = frontAudioBuffer->GetNumSamples() + needSamples;
		rateBlockBuffer.push((SWORD*)frontAudioBuffer->GetData()+used, left*numChannels);
	}
	audioBuffer->SetTimestamp(encoderPTS);	
	availableNumSamples -= requiredNumSamples;
	encoderPTS += frameSize;
	pthread_mutex_unlock(&mutex);
	return audioBuffer;
}

int AudioPipe::ClearBuffer()
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Clear data
	rateBlockBuffer.clear();
	queue.clear();

	//Unlock
	pthread_mutex_unlock(&mutex);
	
	return 1;
}