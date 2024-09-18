#include <string.h>
#include <libavcodec/avcodec.h>
#include "log.h"
#include "speexcodec.h"

SpeexEncoder::SpeexEncoder(const Properties &properties)
	: audioFrame(std::make_shared<AudioFrame>(AudioCodec::SPEEX16))
{
	///Set type
	type =  AudioCodec::SPEEX16;
	//Set number of input frames for codec
	numFrameSamples = 320;
	//+------+---------------+-------------------+------------------------+
	//| mode | Speex quality | wideband bit-rate |     ultra wideband     |
	//|      |               |                   |        bit-rate        |
	//+------+---------------+-------------------+------------------------+
	//|   0  |       0       |    3.95 kbit/s    |       5.75 kbit/s      |
	//|   1  |       1       |    5.75 kbit/s    |       7.55 kbit/s      |
	//|   2  |       2       |    7.75 kbit/s    |       9.55 kbit/s      |
	//|   3  |       3       |    9.80 kbit/s    |       11.6 kbit/s      |
	//|   4  |       4       |    12.8 kbit/s    |       14.6 kbit/s      |
	//|   5  |       5       |    16.8 kbit/s    |       18.6 kbit/s      |
	//|   6  |       6       |    20.6 kbit/s    |       22.4 kbit/s      |
	//|   7  |       7       |    23.8 kbit/s    |       25.6 kbit/s      |
	//|   8  |       8       |    27.8 kbit/s    |       29.6 kbit/s      |
	//|   9  |       9       |    34.2 kbit/s    |       36.0 kbit/s      |
	//|  10  |       10      |    42.2 kbit/s    |       44.0 kbit/s      |
	//+------+---------------+-------------------+------------------------+

	//Set default quality
	int quality = properties.GetProperty("speex.quality",5);

	// init encoder in wide band
	encoder = speex_encoder_init(&speex_wb_mode);
	
	//Set quality
	speex_encoder_ctl(encoder, SPEEX_SET_QUALITY, &quality);

	//Update rate
	int rate = 16000;
	speex_encoder_ctl(encoder, SPEEX_SET_SAMPLING_RATE, &rate);
	
	//Init bits
	speex_bits_init(&encbits);

	// get frame sizes
	speex_mode_query(&speex_wb_mode, SPEEX_MODE_FRAME_SIZE, &numFrameSamples);
	audioFrame->DisableSharedBuffer();

	Log("-Speex: Open codec with %d numFrameSamples\n",numFrameSamples);
}

SpeexEncoder::~SpeexEncoder()
{
	//Destroy objects
	speex_bits_destroy(&encbits);
	speex_encoder_destroy(encoder);
}

DWORD SpeexEncoder::TrySetRate(DWORD rate, DWORD numChannels)
{
	//return real rate
	return numChannels==1 ? GetRate() : 0;
}

DWORD SpeexEncoder::GetRate()
{
	int rate;
	//Update rate
	speex_encoder_ctl(encoder, SPEEX_GET_SAMPLING_RATE, &rate);
	//return it
	return rate;
}

AudioFrame::shared SpeexEncoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{

	const SWORD *in = audioBuffer->GetData();
	int inLen = audioBuffer->GetNumSamples() * audioBuffer->GetNumChannels();
	int outLen = audioFrame->GetMaxMediaLength();
	if (!in || inLen <= 0)
		return nullptr;

	//check lengths
	if ((inLen != numFrameSamples) || (outLen < numFrameSamples))
	{
		Error("-speex encode ((inLen[%d] != numFrameSamples[%d]) || (outLen[%d] < enc_frame_size[%d]))\n", inLen, numFrameSamples, outLen, numFrameSamples);
		return nullptr;
	}

	//Reset
	speex_bits_reset(&encbits);
	//Encode
	speex_encode_int(encoder, (spx_int16_t*)in, &encbits);
	///Paste
	int len = speex_bits_write_whole_bytes(&encbits, (char*)audioFrame->GetData(), outLen);
	audioFrame->SetLength(len);
	return audioFrame;
}

SpeexDecoder::SpeexDecoder()
{
	//Set number of input frames for codec
	numFrameSamples = 160;
	///Set type
	type = AudioCodec::SPEEX16;
	
	// init decoder
	decoder = speex_decoder_init(&speex_wb_mode);
        speex_bits_init(&decbits);
        speex_bits_reset(&decbits);

	// get frame sizes
	speex_decoder_ctl(decoder, SPEEX_GET_FRAME_SIZE, &dec_frame_size);
}

SpeexDecoder::~SpeexDecoder()
{
	speex_bits_destroy(&decbits);
	speex_decoder_destroy(decoder);
}

DWORD SpeexDecoder::TrySetRate(DWORD rate)
{
	return GetRate();
}

int SpeexDecoder::Decode(const AudioFrame::const_shared& audioFrame)
{

	const uint8_t* in = audioFrame ? (uint8_t*)audioFrame->GetData() : nullptr;
	int inLen = audioFrame ? audioFrame->GetLength() : 0;
	//Nothing to decode
	if (!inLen)
		//Exit
		return 0;
	
	//Read bits
	speex_bits_read_from(&decbits, (char*)in, inLen);
	return 1;
}

AudioBuffer::shared SpeexDecoder::GetDecodedAudioFrame()
{
	auto audioBuffer = std::make_shared<AudioBuffer>(dec_frame_size, 1);
	//Decode
	if (speex_decode_int(decoder, &decbits, (spx_int16_t*)audioBuffer->GetData())<0)
	{
		Error("-speex speex_decode_int error");
		return {};
	}
	return audioBuffer;
}