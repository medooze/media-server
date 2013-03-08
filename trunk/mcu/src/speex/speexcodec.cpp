#include <string.h>
#include <libavcodec/avcodec.h>
#include "log.h"
#include "speexcodec.h"

SpeexEncoder::SpeexEncoder()
{
	//Set number of input frames for codec
	numFrameSamples = 160;
	///Set type
	type =  AudioCodec::SPEEX16;

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

	int quality = 5;
	enc_frame_size = 160;
	int err = 0;

	// init encoder in wide band
	encoder = speex_encoder_init(&speex_wb_mode);
	//Set quality
	speex_encoder_ctl(encoder, SPEEX_SET_QUALITY, &quality);
	//The resampler to convert to WB
	resampler = speex_resampler_init(1, 8000, 16000, 10, &err);
	//Init bits
	speex_bits_init(&encbits);
	// get frame sizes
	speex_mode_query(&speex_wb_mode, SPEEX_MODE_FRAME_SIZE, &enc_frame_size);
	int rate = 16000;
	speex_encoder_ctl(encoder, SPEEX_SET_SAMPLING_RATE, &rate);

	// get frame sizes
	speex_mode_query(&speex_wb_mode, SPEEX_MODE_FRAME_SIZE, &enc_frame_size);
}

SpeexEncoder::~SpeexEncoder()
{
	speex_bits_destroy(&encbits);
	speex_encoder_destroy(encoder);
}

int SpeexEncoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	if (!inLen)
		return 0;

	SWORD wbBuffer[512];
	DWORD wblen = 512;

	//check lengths
	if ((inLen != enc_frame_size/2) || (outLen < enc_frame_size)) 
		return Error("-speex encode ((inLen[%d] != numFrameSamples[%d]) || (outLen[%d] < enc_frame_size[%d]))\n", inLen, enc_frame_size/2, outLen, enc_frame_size);

	//Reset
	speex_bits_reset(&encbits);
	//Transrate
	speex_resampler_process_int(resampler,0,(spx_int16_t*)in,(spx_uint32_t*)&inLen,(spx_int16_t*)wbBuffer,(spx_uint32_t*)&wblen);
	//Encode
	speex_encode_int(encoder, (spx_int16_t*)wbBuffer, &encbits);
	///Paste
	return speex_bits_write_whole_bytes(&encbits, (char*)out, outLen);
}


SpeexDecoder::SpeexDecoder()
{
	//Set number of input frames for codec
	numFrameSamples = 160;
	///Set type
	type = AudioCodec::SPEEX16;
	
	int err = 0;
	//The resampler to convert from WB
	wbresampler = speex_resampler_init(1, 16000, 8000, 10, &err);


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
	speex_encoder_destroy(decoder);
}

int SpeexDecoder::Decode (BYTE *in, int inLen, SWORD* out, int outLen)
{
	SWORD wbBuffer[512];
	DWORD wbLen = dec_frame_size;

	//Nothing to decode
	if (!inLen)
		//Exit
		return 0;
	
	//check lengths
	if (outLen < dec_frame_size) 
		return Error("-speex decode outLen[%d] < dec_frame_size[%d] * 2\n", outLen, dec_frame_size);

	//Read bits
	speex_bits_read_from(&decbits, (char*)in, inLen);
	//Decode
	if (speex_decode_int(decoder, &decbits, (spx_int16_t*)wbBuffer)<0)
		return Error("-speex speex_decode_int error");
	//Transrate
	speex_resampler_process_int(wbresampler,0,(spx_int16_t*)wbBuffer,(spx_uint32_t*)&wbLen,(spx_int16_t*)out,(spx_uint32_t*)&outLen);
	//Exit
	return outLen;
}
