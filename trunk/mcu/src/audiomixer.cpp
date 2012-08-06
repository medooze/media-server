#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include "log.h"
#include "tools.h"
#include "audiomixer.h"
#include "pipeaudioinput.h"
#include "pipeaudiooutput.h"

/***********************
* AudioMixer
*	Constructor
************************/
AudioMixer::AudioMixer()
{
	//Not mixing
	mixingAudio = 0;
}

/***********************
* ~AudioMixer
*	Destructor
************************/
AudioMixer::~AudioMixer()
{
}

/***********************************
* startMixingAudio
*	Crea el thread de mezclado de audio
************************************/
void *AudioMixer::startMixingAudio(void *par)
{
	Log("-MixAudioThread [%d]\n",getpid());

	//Obtenemos el parametro
	AudioMixer *am = (AudioMixer *)par;

	//Bloqueamos las se�ales
	blocksignals();
	
	//Ejecutamos
	pthread_exit((void *)am->MixAudio());
}

/***********************************
* MixAudio
*	Mezcla los audios
************************************/
int AudioMixer::MixAudio()
{
	timeval  tv;
	Audios::iterator	it;
	DWORD step = 10;
	QWORD curr = 0;
	QWORD prev = 0;

	//Logeamos
	Log(">MixAudio\n");

	//Init ts
	getUpdDifTime(&tv);

	//Mientras estemos mezclando
	while(mixingAudio)
	{
		//zero the mixer buffer
		memset(mixer_buffer, 0, MIXER_BUFFER_SIZE*sizeof(WORD));

		//Wait until next to process again minus process time
		msleep(step*1000-(getDifTime(&tv)-curr*1000));

		//Get new time
		curr = getDifTime(&tv)/1000;
		
		//Get time elapsed
		QWORD diff = curr-prev;

		//Update
		prev = curr;

		//Get num samples at 8Khz
		DWORD numSamples = diff*8;

		//Block list
		lstAudiosUse.WaitUnusedAndLock();

		//At most the maximum
		if (numSamples>MIXER_BUFFER_SIZE)
			//Set it at most (shoudl never happen)
			numSamples = MIXER_BUFFER_SIZE;

		//First pass: Iterate through the audio inputs and calculate the sum of all streams
		for(it = lstAudios.begin(); it != lstAudios.end(); it++)
		{
			//Get the source
			AudioSource *audio = it->second;

			//And the audio buffer
			WORD *buffer = audio->buffer;

			//Get the samples from the fifo
			audio->len = audio->output->GetSamples(buffer,numSamples);

			//Mix the audio
			for(int i = 0; i < audio->len; i++)
				//MIX
				mixer_buffer[i] += buffer[i];
		}

		// Second pass: Calculate this stream's output
		for(it = lstAudios.begin(); it != lstAudios.end(); it++)
		{
			//Get the source
			AudioSource *audio = it->second;

			//Check audio
			if (!audio)
				//Next
				continue;
			//And the audio buffer
			WORD *buffer = audio->buffer;

			//Calculate the result
			for(int i=0; i<audio->len; i++)
				//We don't want to hear our own signal
				buffer[i] = mixer_buffer[i] - buffer[i];

			//Check length
			if (audio->len<numSamples)
				//Copy the rest
				memcpy(((BYTE*)buffer)+audio->len*sizeof(WORD),((BYTE*)mixer_buffer)+audio->len*sizeof(WORD),(numSamples-audio->len)*sizeof(WORD));

			//PUt the output
			audio->input->PutSamples(buffer,numSamples);
		}

		//Unblock list
		lstAudiosUse.Unlock();
	}

	//Logeamos
	Log("<MixAudio\n");

	return 1;
}

/***********************
* Init
*	Inicializa el mezclado de audio
************************/
int AudioMixer::Init()
{
	// Estamos mzclando
	mixingAudio = true;

	//Y arrancamoe el thread
	createPriorityThread(&mixAudioThread,startMixingAudio,this,0);

	return 1;
}

/***********************
* End
*	Termina el mezclado de audio
************************/
int AudioMixer::End()
{
	Log(">End audiomixer\n");

	//Borramos los audios
	Audios::iterator it;

	//Terminamos con la mezcla
	if (mixingAudio)
	{
		//Terminamos la mezcla
		mixingAudio = 0;

		//Y esperamos
		pthread_join(mixAudioThread,NULL);
	}

	//Lock
	lstAudiosUse.WaitUnusedAndLock();

	//Recorremos la lista
	for (it =lstAudios.begin();it!=lstAudios.end();++it)
	{
		//Obtenemos el audio source
		AudioSource *audio = it->second;

		//Terminamos
		audio->input->End();
		audio->output->End();
		
		//SI esta borramos los objetos
		delete audio->input;
		delete audio->output;
		delete audio;
	}

	//Unlock
	lstAudiosUse.Unlock();
	
	Log("<End audiomixer\n");
	
	return 1;
}

/***********************
* CreateMixer
*	Crea una nuevo source de audio para mezclar
************************/
int AudioMixer::CreateMixer(int id)
{
	Log(">CreateMixer audio [%d]\n",id);

	//Protegemos la lista
	lstAudiosUse.WaitUnusedAndLock();

	//Miramos que si esta
	if (lstAudios.find(id)!=lstAudios.end())
	{
		//Desprotegemos la lista
		lstAudiosUse.Unlock();
		return Error("Audio sourecer already existed\n");
	}

	//Creamos el source
	AudioSource *audio = new AudioSource();

	//POnemos el input y el output
	audio->input  = new PipeAudioInput();
	audio->output = new PipeAudioOutput();
	//Clean buffer
	memset(audio->buffer, 0, MIXER_BUFFER_SIZE*sizeof(WORD));
	audio->len = 0;

	//Y lo a�adimos a la lista
	lstAudios[id] = audio;

	//Desprotegemos la lista
	lstAudiosUse.Unlock();

	//Y salimos
	Log("<CreateMixer audio\n");

	return true;
}

/***********************
* InitMixer
*	Inicializa un audio
*************************/
int AudioMixer::InitMixer(int id)
{
	Log(">Init mixer [%d]\n",id);

	//Protegemos la lista
	lstAudiosUse.IncUse();

	//Buscamos el audio source
	Audios::iterator it = lstAudios.find(id);

	//Si no esta	
	if (it == lstAudios.end())
	{
		//Desprotegemos
		lstAudiosUse.DecUse();
		//Salimos
		return Error("Mixer not found\n");
	}

	//Obtenemos el audio source
	AudioSource *audio = it->second;

	//INiciamos los pipes
	audio->input->Init();
	audio->output->Init();

	//Desprotegemos
	lstAudiosUse.DecUse();

	Log("<Init mixer [%d]\n",id);

	//Si esta devolvemos el input
	return true;
}


/***********************
* EndMixer
*	Finaliza un audio
*************************/
int AudioMixer::EndMixer(int id)
{
	//Protegemos la lista
	lstAudiosUse.IncUse();

	//Buscamos el audio source
	Audios::iterator it = lstAudios.find(id);

	//Si no esta	
	if (it == lstAudios.end())
	{
		//Desprotegemos
		lstAudiosUse.DecUse();
		//Salimos
		return false;
	}

	//Obtenemos el audio source
	AudioSource *audio = it->second;

	//Terminamos
	audio->input->End();
	audio->output->End();

	//Desprotegemos
	lstAudiosUse.DecUse();

	//Si esta devolvemos el input
	return true;;
}

/***********************
* DeleteMixer
*	Borra una fuente de audio
************************/
int AudioMixer::DeleteMixer(int id)
{
	Log("-DeleteMixer audio [%d]\n",id);

	//Protegemos la lista
	lstAudiosUse.WaitUnusedAndLock();

	//Lo buscamos
	Audios::iterator it = lstAudios.find(id);

	//SI no ta
	if (it == lstAudios.end())
	{
		//DDesprotegemos la lista
		lstAudiosUse.Unlock();
		//Salimos
		return Error("Audio source not found\n");
	}

	//Obtenemos el audio source
	AudioSource *audio = it->second;

	//Lo quitamos de la lista
	lstAudios.erase(it);

	//Desprotegemos la lista
	lstAudiosUse.Unlock();

	//SI esta borramos los objetos
	delete audio->input;
	delete audio->output;
	delete audio; 

	return 0;
}

/***********************
* GetInput
*	Obtiene el input para un id
************************/
AudioInput* AudioMixer::GetInput(int id)
{
	//Protegemos la lista
	lstAudiosUse.IncUse();

	//Buscamos el audio source
	Audios::iterator it = lstAudios.find(id);

	//Obtenemos el input
	AudioInput *input = NULL;

	//Si esta
	if (it != lstAudios.end())
		input = it->second->input;

	//Desprotegemos
	lstAudiosUse.DecUse();

	//Si esta devolvemos el input
	return input;
}

/***********************
* GetOutput
*	Obtiene el output para un id
************************/
AudioOutput* AudioMixer::GetOutput(int id)
{
	//Protegemos la lista
	lstAudiosUse.IncUse();

	//Buscamos el audio source
	Audios::iterator it = lstAudios.find(id);

	//Obtenemos el output
	AudioOutput *output = NULL;

	//Si esta	
	if (it != lstAudios.end())
		output = it->second->output;

	//Desprotegemos
	lstAudiosUse.DecUse();

	//Si esta devolvemos el input
	return output;
}

