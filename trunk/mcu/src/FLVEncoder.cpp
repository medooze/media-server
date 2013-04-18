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
	videoCodec = VideoCodec::SORENSON;
	//Set values for default video
	width	= 352;
	height	= 288;
	bitrate = 512;
	fps	= 30;
	intra	= 600;
	//No meta no desc
	meta = NULL;
	frameDesc = NULL;
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

	if (meta)
		delete(meta);
	if (frameDesc)
		delete(frameDesc);

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
	//Init
	listener->onStreamBegin(RTMPMediaStream::id);

	//If we already have metadata
	if (meta)
		//Send it
		listener->onMetaData(RTMPMediaStream::id,meta);
	//Check desc
	if (frameDesc)
		//Send it
		listener->onMediaFrame(RTMPMediaStream::id,frameDesc);
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
	getUpdDifTime(&first);

	//Check if got old meta
	if (meta)
		//Delete
		delete(meta);

	//Create metadata object
	meta = new RTMPMetaData(0);

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
	
	//Set video codecs
	if (videoCodec==VideoCodec::SORENSON)
		//Set number
		prop->AddProperty(L"videocodecid"	,(float)RTMPVideoFrame::FLV1	);	// Number Video codec ID used in the file (see E.4.3.1 for available CodecID values)
	else if (videoCodec==VideoCodec::H264)
		//AVC
		prop->AddProperty(L"videocodecid"	,new AMFString(L"avc1")		);	// Number Video codec ID used in the file (see E.4.3.1 for available CodecID values)
	prop->AddProperty(L"framerate"		,(float)fps			);	// Number Number of frames per second
	prop->AddProperty(L"height"		,(float)height			);	// Number Height of the video in pixels
	prop->AddProperty(L"videodatarate"	,(float)bitrate			);	// Number Video bit rate in kilobits per second
	prop->AddProperty(L"width"		,(float)width			);	// Number Width of the video in pixels
	prop->AddProperty(L"canSeekToEnd"	,new AMFBoolean(false)		);	// Boolean Indicating the last video frame is a key frame

	//Add param
	meta->AddParam(prop);

	//Send metadata
	SendMetaData(meta);

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
	Log(">Encode Audio\n");

	//Start recording
	audioInput->StartRecording();

	//Create encoder
	AudioEncoder *encoder = AudioCodecFactory::CreateEncoder(audioCodec);

	//Check
	if (!encoder)
		//Error
		return Error("Error encoding audio");

	//No first yet
	QWORD ini = 0;

	//Num of samples since ini
	QWORD samples = 0;

	//Mientras tengamos que capturar
	while(encodingAudio)
	{
		RTMPAudioFrame	audio(0,MTU);
		SWORD 		recBuffer[512];

		//Capturamos
		DWORD  recLen = audioInput->RecBuffer(recBuffer,encoder->numFrameSamples);

		//Check len
		if (!recLen)
		{
			//Log
			Debug("-cont\n");
			//Reser timestam
			ini = getDifTime(&first)/1000;
			//And samples
			samples = 0;
			//Skip
			continue;
		}

		//Rencode it
		DWORD len;

		while((len=encoder->Encode(recBuffer,recLen,audio.GetMediaData(),audio.GetMaxMediaSize()))>0)
		{
			//REset
			recLen = 0;

			//Set length
			audio.SetMediaSize(len);

			//Check if it is first frame
			if (!ini)
				//Get initial timestamp
				ini = getDifTime(&first)/1000;

			switch(encoder->type)
			{
				case AudioCodec::SPEEX16:
					//Set RTMP data
					audio.SetAudioCodec(RTMPAudioFrame::SPEEX);
					audio.SetSoundRate(RTMPAudioFrame::RATE11khz);
					audio.SetSamples16Bits(1);
					audio.SetStereo(0);
					//Set timestamp
					audio.SetTimestamp(ini+samples/16);
					//Increase samples
					samples += 320;
					break;
				case AudioCodec::NELLY8:
					//Set RTMP data
					audio.SetAudioCodec(RTMPAudioFrame::NELLY8khz);
					audio.SetSoundRate(RTMPAudioFrame::RATE11khz);
					audio.SetSamples16Bits(1);
					audio.SetStereo(0);
					//Set timestamp
					audio.SetTimestamp(ini+samples/8);
					//Increase samples
					samples += 256;
					break;
				case AudioCodec::NELLY11:
					//Set RTMP data
					audio.SetAudioCodec(RTMPAudioFrame::NELLY);
					audio.SetSoundRate(RTMPAudioFrame::RATE11khz);
					audio.SetSamples16Bits(1);
					audio.SetStereo(0);
					//Set timestamp
					audio.SetTimestamp(ini+samples*1000/11025);
					//Increase samples
					samples += 256;
					break;
			}


			//Lock
			pthread_mutex_lock(&mutex);
			//Send audio
			SendMediaFrame(&audio);
			//unlock
			pthread_mutex_unlock(&mutex);
		}
	}

	//Stop recording
	audioInput->StopRecording();

	//Check codec
	if (encoder)
		//Borramos el codec
		delete(encoder);
	
	//Salimos
        Log("<Encode Audio\n");

	//Exit
	pthread_exit(0);
}

int FLVEncoder::EncodeVideo()
{
	timeval prev;

	//Allocate media frame
	RTMPVideoFrame frame(0,262143);

	//Check codec
	switch(videoCodec)
	{
		case VideoCodec::SORENSON:
			//Ser Video codec
			frame.SetVideoCodec(RTMPVideoFrame::FLV1);
			break;
		case VideoCodec::H264:
			//Ser Video codec
			frame.SetVideoCodec(RTMPVideoFrame::AVC);
			//Set NAL type
			frame.SetAVCType(RTMPVideoFrame::AVCNALU);
			//No delay
			frame.SetAVCTS(0);
			break;
		default:
			return Error("-Wrong codec type %d\n",videoCodec);
	}
	
	//Create the encoder
	VideoEncoder *encoder = VideoCodecFactory::CreateEncoder(videoCodec);

	///Set frame rate
	encoder->SetFrameRate(fps,bitrate,intra);

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

		//Check pic
		if (!pic)
			continue;

		//Check if we need to send intra
		if (sendFPU)
		{
			//Set it
			encoder->FastPictureUpdate();
			//Do not send anymore
			sendFPU = false;
		}

		//Encode next frame
		VideoFrame *encoded = encoder->EncodeFrame(pic,videoInput->GetBufferSize());

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

	
		//If we need desc but yet not have it
		if (!frameDesc && encoded->IsIntra() && videoCodec==VideoCodec::H264)
		{
			//Create new description
			AVCDescriptor desc;
			//Set values
			desc.SetConfigurationVersion(1);
			desc.SetAVCProfileIndication(0x42);
			desc.SetProfileCompatibility(0x80);
			desc.SetAVCLevelIndication(0x0C);
			desc.SetNALUnitLength(3);
			//Get encoded data
			BYTE *data = encoded->GetData();
			//Get size
			DWORD size = encoded->GetLength();
			//get from frame
			desc.AddParametersFromFrame(data,size);
			//Crete desc frame
			frameDesc = new RTMPVideoFrame(getDifTime(&first)/1000,desc);
			//Lock
			pthread_mutex_lock(&mutex);
			//Send it
			SendMediaFrame(frameDesc);
			//unlock
			pthread_mutex_unlock(&mutex);
		}
		
		
		//Set timestamp
		frame.SetTimestamp(getDifTime(&first)/1000);
		//Publish it
		SendMediaFrame(&frame);
		//unlock
		pthread_mutex_unlock(&mutex);
	}

	//Stop the capture
	videoInput->StopVideoCapture();

	//Check
	if (encoder)
		//Exit
		delete(encoder);
	
	//Exit
	return 1;
}
