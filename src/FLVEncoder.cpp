#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "log.h"
#include "FLVEncoder.h"
#include "flv.h"
#include "flv1/flv1codec.h"
#include "audioencoder.h"

FLVEncoder::FLVEncoder()
{
	//Not inited
	inited = 0;
	encodingAudio = 0;
	encodingVideo = 0;
	sendFPU = false;
	//Set default codecs
	audioCodec = AudioCodec::NELLY11;
	//Mutex
	pthread_mutex_init(&mutex,0);
	pthread_cond_init(&cond,0);
}

FLVEncoder::~FLVEncoder()
{
	//Check
	if (inited)
		//End it
		End();
	//Mutex
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

int FLVEncoder::Init(AudioInput* audioInput,VideoInput *videoInput)
{
	//Check if inited
	if (inited)
		//Error
		return 0;
	
	//Store inputs
	this->audioInput = audioInput;
	this->videoInput = videoInput;

	//Y aun no estamos mandando nada
	encodingAudio = 0;
	encodingVideo = 0;

	//We are initer
	inited = 1;

	return 1;

}

int FLVEncoder::End()
{
	//if inited
	if (!inited)
		//error
		return 0;

	//Stop Encodings
	StopEncoding();

	//Not inited
	inited = 0;

	return 1;
}

/***************************************
* startencodingAudio
*	Helper function
***************************************/
void * FLVEncoder::startEncodingAudio(void *par)
{
	FLVEncoder *enc = (FLVEncoder *)par;
	blocksignals();
	Log("Encoding FLV audio [%d]\n",getpid());
	pthread_exit((void *)enc->EncodeAudio());
}

/***************************************
* startencodingAudio
*	Helper function
***************************************/
void * FLVEncoder::startEncodingVideo(void *par)
{
	FLVEncoder *enc = (FLVEncoder *)par;
	blocksignals();
	Log("Encoding FLV video [%d]\n",getpid());
	pthread_exit((void *)enc->EncodeVideo());
}

DWORD FLVEncoder::AddMediaListener(RTMPMediaStream::Listener *listener)
{
	//Call parent
	RTMPMediaStream::AddMediaListener(listener);
	//Send FPU
	sendFPU = true;
}
/***************************************
* StartEncoding
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int FLVEncoder::StartEncoding()
{
	Log(">Start encoding FLV\n");

	//Si estabamos mandando tenemos que parar
	if (encodingAudio || encodingVideo)
		//paramos
		StopEncoding();

	//We are enconding
	encodingAudio = 1;
	encodingVideo = 1;

	//Set init time
	getUpdDifTime(&ini);

	//Create metadata object
	RTMPMetaData *meta = new RTMPMetaData(0);

	//Set name
	meta->AddParam(new AMFString(L"@setDataFrame"));
	//Set name
	meta->AddParam(new AMFString(L"onMetaData"));

	//Create properties string
	AMFEcmaArray *prop = new AMFEcmaArray();

	//Set audio properties
	switch(audioCodec)
	{
		case AudioCodec::SPEEX16:
			prop->AddProperty(L"audiocodecid"	,(float)RTMPAudioFrame::SPEEX		);	//Number Audio codec ID used in the file (see E.4.2.1 for available SoundFormat values)
			prop->AddProperty(L"audiosamplerate"	,(float)16000.0				);	// Number Frequency at which the audio stream is replayed
			break;
		case AudioCodec::NELLY11:
			prop->AddProperty(L"audiocodecid"	,(float)RTMPAudioFrame::NELLY		);	//Number Audio codec ID used in the file (see E.4.2.1 for available SoundFormat values)
			prop->AddProperty(L"audiosamplerate"	,(float)11025.0				);	// Number Frequency at which the audio stream is replayed
			break;
		case AudioCodec::NELLY8:
			prop->AddProperty(L"audiocodecid"	,(float)RTMPAudioFrame::NELLY8khz	);	//Number Audio codec ID used in the file (see E.4.2.1 for available SoundFormat values)
			prop->AddProperty(L"audiosamplerate"	,(float)8000.0				);	// Number Frequency at which the audio stream is replayed
			break;
	}

	prop->AddProperty(L"stereo"		,new AMFBoolean(false)		);	// Boolean Indicating stereo audio
	prop->AddProperty(L"audiodelay"		,0.0				);	// Number Delay introduced by the audio codec in seconds
	//prop->AddProperty(L"audiodatarate"	,(float)8000			);	// Number Audio bit rate in kilobits per second
	//prop->AddProperty(L"audiosamplesize"	,160.0				);	// Number Resolution of a single audio sample

	//Set video codecs
	prop->AddProperty(L"videocodecid"	,(float)RTMPVideoFrame::FLV1	);	// Number Video codec ID used in the file (see E.4.3.1 for available CodecID values)
	prop->AddProperty(L"framerate"		,(float)30			);	// Number Number of frames per second
	prop->AddProperty(L"height"		,(float)288			);	// Number Height of the video in pixels
	prop->AddProperty(L"videodatarate"	,(float)512			);	// Number Video bit rate in kilobits per second
	prop->AddProperty(L"width"		,(float)352			);	// Number Width of the video in pixels
	prop->AddProperty(L"canSeekToEnd"	,new AMFBoolean(false)		);	// Boolean Indicating the last video frame is a key frame

	//Add param
	meta->AddParam(prop);

	//Dump
	meta->Dump();

	//Send metadata
	SendMetaData(meta);

	//And delete it
	delete(meta);

	//Start thread
	createPriorityThread(&encodingAudioThread,startEncodingAudio,this,1);
	//Start thread
	createPriorityThread(&encodingVideoThread,startEncodingVideo,this,1);

	Log("<Stop encoding FLV [%d]\n",encodingAudio);

	return 1;
}

/***************************************
* StopEncoding
* 	Termina el envio
****************************************/
int FLVEncoder::StopEncoding()
{
	Log(">Stop Encoding FLV\n");

	//Esperamos a que se cierren las threads de envio
	if (encodingAudio)
	{
		//paramos
		encodingAudio=0;

		//Y esperamos
		pthread_join(encodingAudioThread,NULL);
	}

	//Esperamos a que se cierren las threads de envio
	if (encodingVideo)
	{
		//paramos
		encodingVideo=0;

		//Y esperamos
		pthread_join(encodingVideoThread,NULL);	
	}

	Log("<Stop Encoding FLV\n");

	return 1;
}

/*******************************************
* Encode
*	Capturamos el audio y lo mandamos
*******************************************/
int FLVEncoder::EncodeAudio()
{
	RTMPAudioFrame	frame(0,512);
	SWORD 		recBuffer[512];

	Log(">Encode Audio\n");

	//Empezamos a grabar
	audioInput->StartRecording();

	//Create encoder
	AudioCodec *encoder = AudioCodec::CreateCodec(audioCodec);

	//Mientras tengamos que capturar
	while(encodingAudio)
	{
		DWORD numSamples = encoder->numFrameSamples;
		//Capturamos 20ms
		if (audioInput->RecBuffer(recBuffer,numSamples)==0)
			continue;

		DWORD frameLength = 0;

		while((frameLength = encoder->Encode(recBuffer,numSamples,frame.GetMediaData(),frame.GetMaxMediaSize()))>0)
		{
			//REset
			numSamples = 0;

			//Set length
			frame.SetMediaSize(frameLength);

			switch(encoder->type)
			{
				case AudioCodec::SPEEX16:
					//Set RTMP data
					frame.SetAudioCodec(RTMPAudioFrame::SPEEX);
					frame.SetSoundRate(RTMPAudioFrame::RATE11khz);
					frame.SetSamples16Bits(1);
					frame.SetStereo(0);
					break;
				case AudioCodec::NELLY8:
					//Set RTMP data
					frame.SetAudioCodec(RTMPAudioFrame::NELLY8khz);
					frame.SetSoundRate(RTMPAudioFrame::RATE11khz);
					frame.SetSamples16Bits(1);
					frame.SetStereo(0);
					break;
				case AudioCodec::NELLY11:
					//Set RTMP data
					frame.SetAudioCodec(RTMPAudioFrame::NELLY);
					frame.SetSoundRate(RTMPAudioFrame::RATE11khz);
					frame.SetSamples16Bits(1);
					frame.SetStereo(0);
					break;
			}
			
			//Set timestamp
			frame.SetTimestamp(getDifTime(&ini)/1000);

			//Lock
			pthread_mutex_lock(&mutex);
			//Publish
			SendMediaFrame(&frame);
			//unlock
			pthread_mutex_unlock(&mutex);
		}
	}

	Log("-Encode Audio cleanup[%d]\n",encodingAudio);

	//Paramos de grabar por si acaso
	audioInput->StopRecording();

	//Logeamos
	Log("-Deleting codec\n");

	//Borramos el codec
	delete(encoder);
	
	//Salimos
        Log("<Encode Audio\n");

	pthread_exit(0);
}

int FLVEncoder::EncodeVideo()
{
	timeval prev;
	//Set width and height
	DWORD width = 352;
	DWORD height = 288;
	DWORD fps = 30;

	//Allocate media frame
	RTMPVideoFrame frame(0,65535);
	//Ser Video codec
	frame.SetVideoCodec(RTMPVideoFrame::FLV1);
	//Create the encoder
	VideoEncoder *encoder = VideoCodecFactory::CreateEncoder(VideoCodec::SORENSON);

	///Set frame rate
	encoder->SetFrameRate(fps,512,600);

	//Set dimensions
	encoder->SetSize(width,height);

	//Start capturing
	videoInput->StartVideoCapture(width,height,fps);

	//No wait for first
	DWORD frameTime = 0;

	//Mientras tengamos que capturar
	while(encodingVideo)
	{
		//Nos quedamos con el puntero antes de que lo cambien
		BYTE* pic=videoInput->GrabFrame();

		//Check if we need to send intra
		if (sendFPU)
		{
			//Set it
			encoder->FastPictureUpdate();
			//Do not send anymore
			sendFPU = false;
		}

		//Encode next frame
		VideoFrame *encoded = encoder->EncodeFrame(pic,width*height*3/2);

		//Check
		if (!encoded)
			break;

		//Check size
		if (frame.GetMaxMediaSize()<encoded->GetLength())
		{
			//Not enougth space
			Error("Not enought space to copy FLV encodec frame [frame:%d,encoded:%d",frame.GetMaxMediaSize(),encoded->GetLength());
			//NExt
			continue;
		}

		//Check
		if (frameTime)
		{
			timespec ts;
			//Lock
			pthread_mutex_lock(&mutex);
			//Calculate timeout
			calcAbsTimeout(&ts,&prev,frameTime);
			//Wait next or stopped
			int canceled  = !pthread_cond_timedwait(&cond,&mutex,&ts);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Check if we have been canceled
			if (canceled)
				//Exit
				break;
		}
		//Set sending time of previous frame
		getUpdDifTime(&prev);

		//Set next one
		frameTime = 1000/fps;
		
		//Get full frame
		frame.SetVideoFrame(encoded->GetData(),encoded->GetLength());

		//Set buffer size
		frame.SetMediaSize(encoded->GetLength());

		//Check type
		if (encoded->IsIntra())
			//Set type
			frame.SetFrameType(RTMPVideoFrame::INTRA);
		else
			//Set type
			frame.SetFrameType(RTMPVideoFrame::INTER);

		//Let the connection set the timestamp
		frame.SetTimestamp(getDifTime(&ini)/1000);

		//Lock
		pthread_mutex_lock(&mutex);
		//Publish it
		SendMediaFrame(&frame);
		//unlock
		pthread_mutex_unlock(&mutex);
	}

	//Stop the capture
	videoInput->StopVideoCapture();

	//Exit
	delete(encoder);
	
	//Exit
	return 1;
}
