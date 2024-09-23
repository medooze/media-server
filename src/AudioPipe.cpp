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
		// Clear playing buffer
		rateBlockBuffer.clear();
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
	auto size = audioBuffer->GetNumSamples();

	//Don't do anything if nobody is listening
	if (!recording)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Ok
		return size;
	}
	if (decoderPTSOffset == std::numeric_limits<QWORD>::max()) 
		decoderPTSOffset = audioBuffer->GetTimestamp();
	
	//Check if we are transtrating
	SWORD resampled[8192];
	if (transrater.IsOpen())
	{
		DWORD resampledSize = 8192 / numChannels;
		//Proccess
		if (!transrater.ProcessBuffer(buffer, size, resampled, &resampledSize))
		{
			//Error
			pthread_mutex_unlock(&mutex);
			return Error("-AudioPipe::PlayBuffer() could not transrate\n");
		}
		auto decoderPTS = audioBuffer->GetTimestamp();
		// scale decoder pts based on recordRate which is the clockrate for encoded audio
		// so that the pts stored in audio buffer is always in encoded audio time base
		auto scaledDecoderPTS = std::round<uint64_t>((decoderPTS-decoderPTSOffset) * (recordRate / (double)playRate)) + decoderPTSOffset;
		audioBuffer->SetTimestamp(scaledDecoderPTS);
		auto len = audioBuffer->SetSamples(resampled, resampledSize, 0, true);
		if (len < resampledSize)
		{
			pthread_mutex_unlock(&mutex);
			return Error("-AudioPipe::PlayBuffer() less resampled audio data copied, actual=%d - expected=%d\n", len, resampledSize);
		}	
	}
	queue.push_back(audioBuffer);
	availableNumSamples += audioBuffer->GetNumSamples()*numChannels;
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
	auto buffer = const_cast<SWORD*>(audioBuffer->GetData());
	QWORD requiredNumSamples = frameSize * numChannels; 
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
	AudioBuffer::const_shared frontAudioBuffer;
	auto bufferedSamples = rateBlockBuffer.length() / numChannels;
	// num samples per channel read from rateBlockBuffer
	auto readSamples = std::min(bufferedSamples, frameSize);
	// read whatever in rateBlockBuffer first
	rateBlockBuffer.pop(buffer, readSamples*numChannels);
	// num samples per channel needed from audio buffer queue
	int needSamples = frameSize - readSamples;

	auto encoderPTS = bufferedSamples ? rateBlockBufferPTS : queue.front()->GetTimestamp();
	// store prevPTS for pts wraparound/discontinuity detection
	auto prevPTS = bufferedSamples ? rateBlockBufferPTS : queue.front()->GetTimestamp();;
	// numSamples used to check ts discontinuity
	auto numSamples = static_cast<int16_t>(bufferedSamples);
	size_t offset = readSamples * numChannels;
	while(queue.length()>0 && needSamples>0)
	{
		frontAudioBuffer = queue.front();
		auto currDecoderPTS = frontAudioBuffer->GetTimestamp();
		auto currDecoderSamples = static_cast<int>(frontAudioBuffer->GetNumSamples());
		//timestamp wraparound
		if(currDecoderPTS < prevPTS)
		{
			int diff = currDecoderPTS-bufferedSamples;
			// only need to update rateBlockBufferPTS if ts wraparound happened between rateBlockBuffer and audioBuffers.
			if(diff>0 && prevPTS == rateBlockBufferPTS && bufferedSamples) 
			{
				rateBlockBufferPTS = diff;
				encoderPTS = rateBlockBufferPTS;
			}
		}
		// in case of pts discontinuity
		else if(currDecoderPTS > prevPTS + 2*numSamples) 
			break;
		
		prevPTS = currDecoderPTS;
		numSamples = currDecoderSamples;

		audioBuffer->SetSamples((SWORD*)frontAudioBuffer->GetData(), std::min(needSamples, currDecoderSamples), offset);
		offset += std::min(needSamples, currDecoderSamples) * numChannels;
		needSamples -= frontAudioBuffer->GetNumSamples();
		queue.pop_front();
	}

	audioBuffer->SetTimestamp(encoderPTS);
	if(needSamples > 0)
	{
		availableNumSamples -= requiredNumSamples-needSamples*numChannels;
		pthread_mutex_unlock(&mutex);
		return audioBuffer;
	}
	else
	{
		if(frontAudioBuffer)
		{
			int left = -needSamples, used = frontAudioBuffer->GetNumSamples() + needSamples;
			rateBlockBuffer.push((SWORD*)frontAudioBuffer->GetData()+used*numChannels, left*numChannels);
			rateBlockBufferPTS = frontAudioBuffer->GetTimestamp() + used;
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

	//Unlock
	pthread_mutex_unlock(&mutex);
	
	return 1;
}