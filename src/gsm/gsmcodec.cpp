#include <string.h>
#include "gsmcodec.h"

#define GSM_FRAME_LENGTH 33
GSMEncoder::GSMEncoder(const Properties &properties) : audioFrame(AudioCodec::GSM)
{
	int     fast       = 0;
	int     wav        = 0;

	type=AudioCodec::GSM;
	numFrameSamples=160;
	g = gsm_create();

//	gsm_option(g, GSM_OPT_FAST,    &fast);
	gsm_option(g, GSM_OPT_WAV49,   &wav);
	audioFrame.DisableSharedBuffer();
}

GSMEncoder::~GSMEncoder()
{
	gsm_destroy(g);
}

AudioFrame* GSMEncoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{

	const SWORD *in = audioBuffer->GetData();
	int inLen = audioBuffer->GetNumSamples() * audioBuffer->GetNumChannels();
	if(inLen != numFrameSamples || audioFrame.GetMaxMediaLength() < GSM_FRAME_LENGTH) 
		return nullptr;

	//Codificamos
	gsm_encode(g,(gsm_signal *)in,(gsm_byte *)audioFrame.GetData());
	audioFrame.SetLength(GSM_FRAME_LENGTH);
	return &audioFrame;
}


GSMDecoder::GSMDecoder()
{
	int     fast       = 0;
	int     wav        = 0;

	type=AudioCodec::GSM;
	numFrameSamples=160;
	g = gsm_create();

//	gsm_option(g, GSM_OPT_FAST,    &fast);
	gsm_option(g, GSM_OPT_WAV49,   &wav);
}

GSMDecoder::~GSMDecoder()
{
	gsm_destroy(g);
}

int GSMDecoder::Decode(const AudioFrame::const_shared& audioFrame)
{
	//Dependiendo de la longitud tenemos un tipo u otro
	int inLen = audioFrame->GetLength();
	uint8_t* in =  const_cast<uint8_t*>(audioFrame->GetData());
	if(!in || inLen==0) 
		return 0;
	audioFrameInfo.first = in;
	audioFrameInfo.second = inLen;
	return 1;
}

AudioBuffer::shared GSMDecoder::GetDecodedAudioFrame()
{
	int inLen = audioFrameInfo.second;
	const uint8_t* in = audioFrameInfo.first;
	if (inLen==33)
	{
		int outLen = 160;
		auto audioBuffer = std::make_shared<AudioBuffer>(outLen, 1);
		//Decodificamso
		if (gsm_decode(g,(gsm_byte *)in,(gsm_signal *)audioBuffer->GetData())<0)
			return {};
			
		return audioBuffer;
	} 
	else if (inLen==65) 
	{

		//ponemos el modo wav
		int wav=1;
		gsm_option(g, GSM_OPT_WAV49,   &wav);
		int outLen = 160*2;
		auto audioBuffer = std::make_shared<AudioBuffer>(outLen, 1);
		//Decodificamos el primero
		if (gsm_decode(g,(gsm_byte *)in,(gsm_signal *)audioBuffer->ConsumeData(160))<0)
			return {};

		//Y el segundo
		if (gsm_decode(g,(gsm_byte *)&in[33],(gsm_signal *)audioBuffer->ConsumeData(160))<0)
			return {};

		return audioBuffer;	
	} 
	return {};
}
