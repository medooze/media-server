#include <string.h>
#include "gsmcodec.h"


GSMEncoder::GSMEncoder(const Properties &properties)
{
	int     fast       = 0;
	int     wav        = 0;

	type=AudioCodec::GSM;
	numFrameSamples=160;
	frameLength=33;
	g = gsm_create();

//	gsm_option(g, GSM_OPT_FAST,    &fast);
	gsm_option(g, GSM_OPT_WAV49,   &wav);
}

GSMEncoder::~GSMEncoder()
{
	gsm_destroy(g);
}

int GSMEncoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	//Comprobamos las longitudes
	if ((inLen!=numFrameSamples) || (outLen<frameLength))
		return 0;

	//Codificamos
	gsm_encode(g,(gsm_signal *)in,(gsm_byte *)out);

	return frameLength;
}


GSMDecoder::GSMDecoder()
{
	int     fast       = 0;
	int     wav        = 0;

	type=AudioCodec::GSM;
	numFrameSamples=160;
	frameLength=33;
	g = gsm_create();

//	gsm_option(g, GSM_OPT_FAST,    &fast);
	gsm_option(g, GSM_OPT_WAV49,   &wav);
}

GSMDecoder::~GSMDecoder()
{
	gsm_destroy(g);
}

int GSMDecoder::Decode (BYTE *in,int inLen,SWORD* out,int outLen)
{
	//Dependiendo de la longitud tenemos un tipo u otro
	if (inLen==33)
	{
		//GSM Clasico
		if (outLen<160)	
			return 0;

		//Decodificamso
		if (gsm_decode(g,(gsm_byte *)in,(gsm_signal *)out)<0)
			return 0;

		return 160;
	} else if (inLen==65) {

		//ponemos el modo wav
		int wav=1;
		gsm_option(g, GSM_OPT_WAV49,   &wav);
		//GSM de M$ vienen 2 paquetes seguidos
		if (outLen<160*2)
			return 0;

		//Decodificamos el primero
		if (gsm_decode(g,(gsm_byte *)in,(gsm_signal *)out)<0)
			return 0;

		//Y el segundo
		if (gsm_decode(g,(gsm_byte *)&in[33],(gsm_signal *)&out[160])<0)
			return 0;

		return 160*2;
				
	} 

	return 0;
}
