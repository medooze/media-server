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
	// since number of samples in decoded audio buffer and encoder frame size may differ,
	// rateBlockBuffer is used to store the leftover. So always read number of samples
	// from rateBlockBuffer first, and if more samples are needed, then read from queue.
	AudioBuffer::const_shared frontAudioBuffer;
	// readSamples: total num samples to read from rateBlockBuffer
	auto readSamples = std::min(rateBlockBuffer.length(), requiredNumSamples);
	// needSamples: num samples needed from queue after consuming rateBlockBuffer
	auto needSamples = requiredNumSamples - readSamples;
	// numSamples: used to check pts discontinuity/wraparound
	auto numSamples = static_cast<int16_t>(rateBlockBuffer.length() / numChannels);

	// if rateBlockBuffer has data, recPTS is rateBlockBufferPTS
	// otherwise, recPTS is timestamp of first audio buffer from the queue
	auto recPTS = rateBlockBuffer.length() > 0 ? rateBlockBufferPTS : queue.front()->GetTimestamp();
	// store prevPTS for pts wraparound/discontinuity detection
	auto prevPTS = recPTS;

	// read whatever in rateBlockBuffer first
	rateBlockBuffer.pop(const_cast<int16_t*>(audioBuffer->GetData()), readSamples);
	// update write offset since readSamples already written into the buffer
	auto offset = readSamples;

	while (queue.length()>0 && needSamples>0)
	{
		frontAudioBuffer = queue.front();
		auto currPlayPTS = frontAudioBuffer->GetTimestamp();
		auto currPlaySamples = static_cast<int>(frontAudioBuffer->GetNumSamples());
		// timestamp wraparound
		if (currPlayPTS < prevPTS)
		{
			int diff = currPlayPTS-numSamples;
			// only need to update rateBlockBufferPTS if ts wraparound happened between rateBlockBuffer and audioBuffers.
			if(diff>0 && prevPTS == rateBlockBufferPTS && numSamples) 
			{
				rateBlockBufferPTS = diff;
				recPTS = rateBlockBufferPTS;
			}
		}
		// in case of pts discontinuity
		else if (currPlayPTS > prevPTS + 2*numSamples) 
			break;
		
		prevPTS = currPlayPTS;
		numSamples = currPlaySamples;
		// num samples to be consumed 
		auto consumedSamples = std::min(needSamples, static_cast<int>(currPlaySamples*numChannels));
		audioBuffer->SetSamples((SWORD*)frontAudioBuffer->GetData(), consumedSamples, offset);
		offset += consumedSamples;
		needSamples -= frontAudioBuffer->GetNumSamples()*numChannels;
		queue.pop_front();
	}

	audioBuffer->SetTimestamp(recPTS);
	// pts discontinuity detected
	if (needSamples > 0)
	{
		availableNumSamples -= requiredNumSamples-needSamples;
		pthread_mutex_unlock(&mutex);
		// in case of discontinuity, the audio buffer may contain some amount of silence.
		return audioBuffer;
	}
	else
	{
		if (frontAudioBuffer)
		{
			// used: number of samples that's already written to audio buffer in while loop
			int used = frontAudioBuffer->GetNumSamples()*numChannels + needSamples;
			// push leftover to rateBlockBuffer
			rateBlockBuffer.push((SWORD*)frontAudioBuffer->GetData()+used, -needSamples);
			// update rateBlockBufferPTS
			rateBlockBufferPTS = frontAudioBuffer->GetTimestamp() + used/numChannels;
		}
		else
			rateBlockBufferPTS += frameSize;
	}
	availableNumSamples -= requiredNumSamples;
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