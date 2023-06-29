#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "log.h"
#include "rtmp/flvencoder.h"
#include "rtmp/flv.h"
#include "flv1/flv1codec.h"
#include "audioencoder.h"
#include "aac/aacconfig.h"
#include "AudioCodecFactory.h"
#include "VideoCodecFactory.h"

FLVEncoder::FLVEncoder(DWORD id) : RTMPMediaStream(id)
{
	//Not inited
	inited = 0;
	encodingAudio = 0;
	encodingVideo = 0;
	sendFPU = false;
	//Set default codecs
	audioCodec = AudioCodec::NELLY11;
	videoCodec = VideoCodec::SORENSON;
	//Add aac properties
	audioProperties.SetProperty("aac.samplerate","48000");
	audioProperties.SetProperty("aac.bitrate","128000");
	//Set values for default video
	width	= GetWidth(CIF);
	height	= GetHeight(CIF);
	bitrate = 512;
	fps	= 30;
	intra	= 600;
	//No meta no desc
	meta = NULL;
	frameDesc = NULL;
	aacSpecificConfig = NULL;
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
	if (aacSpecificConfig)
		delete(aacSpecificConfig);

	//Mutex
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

int FLVEncoder::Init(AudioInput* audioInput,VideoInput *videoInput,const Properties &properties)
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
	
	//Check audio codec
	if (properties.HasProperty("audio.codec"))
	{
		//Get it
		const char* codec = properties.GetProperty("audio.codec");
		//Get audio codec
		audioCodec = AudioCodec::GetCodecForName(codec);
	}
	
	//Check video codec
	if (properties.HasProperty("video.codec"))
	{
		//Get it
		const char* codec = properties.GetProperty("video.codec");
		//Get audio codec
		videoCodec = VideoCodec::GetCodecForName(codec);
	}
	
	//Set values for video
	width	= properties.GetProperty("video.width",width);
	height	= properties.GetProperty("video.height",height);
	bitrate = properties.GetProperty("video.bitrate",bitrate);
	fps	= properties.GetProperty("video.fps",fps);
	intra	= properties.GetProperty("video.intra",intra);

	//Set audio properties
	audioProperties.SetProperty("aac.samplerate",properties.GetProperty("audio.codec.aac.samplerate","48000"));
	audioProperties.SetProperty("aac.bitrate",properties.GetProperty("audio.codec.aac.bitrate","288000"));
	
	//Set video properties
	videoProperties.SetProperty("streaming","true");

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
	
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Remove all rmpt media listenere
	RTMPMediaStream::RemoveAllMediaListeners();
	//Clear media listeners
	mediaListeners.clear();
	//Unlock
	pthread_mutex_unlock(&mutex);

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
	Log("Encoding FLV audio [%p]\n",pthread_self());
	enc->EncodeAudio();
	//Exit
	return NULL;;
}

/***************************************
* startencodingAudio
*	Helper function
***************************************/
void * FLVEncoder::startEncodingVideo(void *par)
{
	FLVEncoder *enc = (FLVEncoder *)par;
	blocksignals();
	Log("Encoding FLV video [%p]\n",pthread_self());
	enc->EncodeVideo();
	//Exit
	return NULL;
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
	//Check audio desc
	if (aacSpecificConfig)
		//Send it
		listener->onMediaFrame(RTMPMediaStream::id,aacSpecificConfig);
	//Send FPU
	sendFPU = true;
}

DWORD FLVEncoder::AddMediaFrameListener(MediaFrame::Listener* listener)
{
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Apend
	mediaListeners.insert(listener);
	//Get number of listeners
	DWORD num = listeners.size();
	//Unlock
	pthread_mutex_unlock(&mutex);
	//return number of listeners
	return num;
}

DWORD FLVEncoder::RemoveMediaFrameListener(MediaFrame::Listener* listener)
{
	//Lock mutexk
	pthread_mutex_lock(&mutex);
	//Find it
	MediaFrameListeners::iterator it = mediaListeners.find(listener);
	//If present
	if (it!=mediaListeners.end())
		//erase it
		mediaListeners.erase(it);
	//Get number of listeners
	DWORD num = mediaListeners.size();
	//Unlock
	pthread_mutex_unlock(&mutex);
	//return number of listeners
	return num;
}

int FLVEncoder::StartEncoding()
{
	Log(">Start encoding FLV [id:%d]\n",id);
	
	//Si estabamos mandando tenemos que parar
	if (encodingAudio || encodingVideo)
		//paramos
		StopEncoding();

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

	//If got audio
	if (audioInput)
	{
		//We are enconding
		encodingAudio = 1;
		//Start thread
		createPriorityThread(&encodingAudioThread,startEncodingAudio,this,1);
	}
	
	//If got video
	if (videoInput)
	{
		//We are enconding
		encodingVideo = 1;
		//Start thread
		createPriorityThread(&encodingVideoThread,startEncodingVideo,this,1);
	}

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

	Debug("-Stop ending audio\n");
	
	//Esperamos a que se cierren las threads de envio
	if (encodingAudio)
	{
		//paramos
		encodingAudio=0;

		//Cancel any pending grab
		audioInput->CancelRecBuffer();

		//Y esperamos
		pthread_join(encodingAudioThread,NULL);
	}

	Debug("-Stop ending video\n");
	
	//Esperamos a que se cierren las threads de envio
	if (encodingVideo)
	{
		//paramos
		encodingVideo=0;

		//Cancel any pending grab
		videoInput->CancelGrabFrame();
		
		//Signal cancel
		pthread_cond_signal(&cond);

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
	RTMPAudioFrame	audio(0,4096);

	//Start
	Log(">Encode Audio\n");

	//Fill frame preperties
	switch(audioCodec)
	{
		case AudioCodec::SPEEX16:
			//Set RTMP data
			audio.SetAudioCodec(RTMPAudioFrame::SPEEX);
			audio.SetSoundRate(RTMPAudioFrame::RATE11khz);
			audio.SetSamples16Bits(1);
			audio.SetStereo(0);
			break;
		case AudioCodec::NELLY8:
			//Set RTMP data
			audio.SetAudioCodec(RTMPAudioFrame::NELLY8khz);
			audio.SetSoundRate(RTMPAudioFrame::RATE11khz);
			audio.SetSamples16Bits(1);
			audio.SetStereo(0);
			break;
		case AudioCodec::NELLY11:
			//Set RTMP data
			audio.SetAudioCodec(RTMPAudioFrame::NELLY);
			audio.SetSoundRate(RTMPAudioFrame::RATE11khz);
			audio.SetSamples16Bits(1);
			audio.SetStereo(0);
			break;
		case AudioCodec::AAC:
			//Set RTMP data
			// If the SoundFormat indicates AAC, the SoundType should be 1 (stereo) and the SoundRate should be 3 (44 kHz).
			// However, this does not mean that AAC audio in FLV is always stereo, 44 kHz data.
			// Instead, the Flash Player ignores these values and extracts the channel and sample rate data is encoded in the AAC bit stream.
			audio.SetAudioCodec(RTMPAudioFrame::AAC);
			audio.SetSoundRate(RTMPAudioFrame::RATE44khz);
			audio.SetSamples16Bits(1);
			audio.SetStereo(1);
			audio.SetAACPacketType(RTMPAudioFrame::AACRaw);
			break;
		default:
			return Error("-Codec %s not supported\n",AudioCodec::GetNameFor(audioCodec));
	}
	
	//Create encoder
	AudioEncoder *encoder = AudioCodecFactory::CreateEncoder(audioCodec,audioProperties);

	//Check
	if (!encoder)
		//Error
		return Error("Error opening encoder");
	
	//Try to set native rate
	DWORD rate = encoder->TrySetRate(audioInput->GetNativeRate(),1);

	//Create audio frame
	AudioFrame frame(audioCodec);
	
	//Set rate
	frame.SetClockRate(rate);

	//Start recording
	audioInput->StartRecording(rate);

	//Get first
	QWORD ini = 0;

	//Num of samples since ini
	QWORD samples = 0;

	//Allocate samlpes
	SWORD* recBuffer = (SWORD*) malloc(encoder->numFrameSamples*sizeof(SWORD));

	//Check codec
	if (audioCodec==AudioCodec::AAC)
	{
		//Create AAC config frame
		aacSpecificConfig = new RTMPAudioFrame(0,AACSpecificConfig(rate,1));

		//Lock
		pthread_mutex_lock(&mutex);
		//Send audio desc
		SendMediaFrame(aacSpecificConfig);
		//unlock
		pthread_mutex_unlock(&mutex);
	}

	//Mientras tengamos que capturar
	while(encodingAudio)
	{
		//Check clock drift, do not allow to exceed 4 frames
		if (ini+(samples+encoder->numFrameSamples*4)*1000/encoder->GetClockRate()<getDifTime(&first)/1000)
		{
			Log("-RTMPParticipant clock drift, dropping audio and reseting init time\n");
			//Clear buffer
			audioInput->ClearBuffer();
			//Reser timestam
			ini = getDifTime(&first)/1000;
			//And samples
			samples = 0;
		}
		
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

			//Set timestamp
			audio.SetTimestamp(ini+samples*1000/encoder->GetClockRate());

			//Increase samples
			samples += encoder->numFrameSamples;

			//Copy to rtp frame
			frame.SetMedia(audio.GetMediaData(),audio.GetMediaSize());
			//Set frame time
			frame.SetTimestamp(audio.GetTimestamp());
			frame.SetTime(audio.GetTimestamp());
			//Set frame duration
			frame.SetDuration(encoder->numFrameSamples);
			//Clear rtp
			frame.ClearRTPPacketizationInfo();
			//Add rtp packet
			frame.AddRtpPacket(0,len,NULL,0);
			
			//Lock
			pthread_mutex_lock(&mutex);
			//Send audio
			SendMediaFrame(&audio);
			//For each listener
			for(MediaFrameListeners::iterator it = mediaListeners.begin(); it!=mediaListeners.end(); ++it)
				//Send it
				(*it)->onMediaFrame(RTMPMediaStream::id,frame);
			//unlock
			pthread_mutex_unlock(&mutex);
		}
	}

	//Stop recording
	audioInput->StopRecording();

	//Delete buffer
	if (recBuffer)
		//Delete
		free(recBuffer);

	//Check codec
	if (encoder)
		//Borramos el codec
		delete(encoder);
	
	//Salimos
        Log("<Encode Audio\n");
}

int FLVEncoder::EncodeVideo()
{
	timeval prev;

	//Start
	Log(">FLVEncoder  encode video\n");

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
	VideoEncoder *encoder = VideoCodecFactory::CreateEncoder(videoCodec,videoProperties);

	///Set frame rate
	encoder->SetFrameRate(fps,bitrate,intra);

	//Set dimensions
	encoder->SetSize(width,height);

	//Start capturing
	videoInput->StartVideoCapture(width,height,fps);

	//The time of the first one
	gettimeofday(&prev,NULL);

	//No wait for first
	DWORD frameTime = 0;

	Log(">FLVEncoder encode vide\n");

	//Mientras tengamos que capturar
	while(encodingVideo)
	{
		//Nos quedamos con el puntero antes de que lo cambien
		auto pic = videoInput->GrabFrame(frameTime);
		
		//Ensure we are still encoding
		if (!encodingVideo)
			break;

		//Check picture
		if (!pic.buffer)
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
		VideoFrame *encoded = encoder->EncodeFrame(pic.buffer,pic.GetBufferSize());

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

		//Set clock rate
		encoded->SetClockRate(1000);
		
		//Set timestamp
		auto now = getDifTime(&first)/1000;
		encoded->SetTimestamp(now);
		encoded->SetTime(now);
		
		//Set next one
		frameTime = 1000/fps;

		//Set duration
		encoded->SetDuration(frameTime);
		
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
			desc.SetNALUnitLengthSizeMinus1(3);
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
		
		//Lock
		pthread_mutex_lock(&mutex);
		//Set timestamp
		frame.SetTimestamp(encoded->GetTimeStamp());
		//Publish it
		SendMediaFrame(&frame);
		//For each listener
		for(MediaFrameListeners::iterator it = mediaListeners.begin(); it!=mediaListeners.end(); ++it)
			//Send it
			(*it)->onMediaFrame(RTMPMediaStream::id,*encoded);
		//unlock
		pthread_mutex_unlock(&mutex);
	}
	Log("-FLVEncoder encode video end of loop\n");

	//Stop the capture
	videoInput->StopVideoCapture();

	//Check
	if (encoder)
		//Exit
		delete(encoder);
	Log("<FLVEncoder encode vide\n");
	
	//Exit
	return 1;
}
