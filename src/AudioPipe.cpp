#include "log.h"
#include "AudioPipe.h"
	
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
		queue.clear();
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

	auto size = audioBuffer->GetNumSamples();

	//Don't do anything if nobody is listening
	if (!recording)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Ok
		return size;
	}
	
	//Check if we are transtrating
	if (transrater.IsOpen())
	{
		AudioBuffer::shared resampled = transrater.ProcessBuffer(audioBuffer);
		if (!resampled)
		{
			pthread_mutex_unlock(&mutex);
			return Error("-AudioPipe::PlayBuffer() could not transrate\n");
		}
		queue.push_back(resampled);
		availableNumSamples += resampled->GetNumSamples()*numChannels;
	}
	else
	{
		queue.push_back(audioBuffer);
		availableNumSamples += audioBuffer->GetNumSamples()*numChannels;
	}
	//Signal rec
	pthread_cond_signal(&cond);
	//Unlock
	pthread_mutex_unlock(&mutex);
	return size;
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
	// audioBuffer has to be created after playing, 
	// otherwise default numChannels = 1 is used which may cause problem when playing stereo
	auto audioBuffer = std::make_shared<AudioBuffer>(frameSize, numChannels);
	auto requiredNumSamples = static_cast<int>(frameSize * numChannels); 
	//Until we have enough samples
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
	
	// Set audio buffer timestamp based on the timestamp of the pending samples or next audio buffer in queue 
	audioBuffer->SetTimestamp(rateBlockBuffer.length() > 0
		? rateBlockBufferPTS
		: queue.front()->GetTimestamp());

	// variables for pts discontinuity check
	auto prevPTS = audioBuffer->GetTimestamp();
	// discontinuity might happen between rateBlockBuffer and front audio buffer of the queue
	auto samples = rateBlockBuffer.length() / numChannels;

	// Read from rateBlockBuffer
	auto readedSamples = std::min(rateBlockBuffer.length(), requiredNumSamples);
	rateBlockBuffer.pop(const_cast<int16_t*>(audioBuffer->GetData()), readedSamples);
	rateBlockBufferPTS += readedSamples;

	while (queue.length()>0 && readedSamples<requiredNumSamples)
	{
		//Get reference to first buffer in queue 
		auto& frontAudioBuffer = queue.front();
		
		if (frontAudioBuffer->GetTimestamp() > 2 * samples + prevPTS)
			break;
		// Udpate variables for ts discontinuity check
		prevPTS = frontAudioBuffer->GetTimestamp();
		samples = frontAudioBuffer->GetNumSamples();

		//The number of samples to read
		auto totalSamplesInBuffer =  static_cast<int>(frontAudioBuffer->GetNumSamples() * numChannels);
		auto neededSamples = std::min(totalSamplesInBuffer, requiredNumSamples - readedSamples);
		// Read from queued buffer
		audioBuffer->SetSamples(const_cast<int16_t*>(frontAudioBuffer->GetData()), neededSamples ,readedSamples);

		// If we have not readed all the available samples
		if (neededSamples < totalSamplesInBuffer) 
		{
			// push leftover to rateBlockBuffer
			rateBlockBuffer.push((SWORD*)frontAudioBuffer->GetData()+neededSamples, totalSamplesInBuffer-neededSamples);
			// update rateBlockBufferPTS
			rateBlockBufferPTS = frontAudioBuffer->GetTimestamp() + neededSamples/numChannels;
		}
		readedSamples += neededSamples;
		// Remove consumed audio buffer from the queue
		queue.pop_front();
	}

	// Remove readed samples from the total available
	availableNumSamples -= readedSamples;

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