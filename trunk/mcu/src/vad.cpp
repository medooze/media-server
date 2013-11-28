#include "vad.h"

#ifdef VADWEBRTC
VAD::VAD()
{
	//Init webrtc vad
	WebRtcVad_InitCore(&inst);
	//Set agrresive mode
	WebRtcVad_set_mode_core(&inst,QUALITY);
}

int VAD::CalcVad(SWORD* frame,DWORD size,DWORD rate)
{
	//Check rate
	if (!IsRateSupported(rate))
		//Error
		return 0;
	
	//No vad
	int vad = 0;

	//Maximun 20 ms at 48khz
	SWORD buffer[960];
	
	//Calcule 20ms at rate
	DWORD len = 20*rate/1000;

	//Push samples
	samples.push(frame,size);

	//While we have enought samples
	while (samples.length()>=len)
	{
		//Get 20 ms of samples
		samples.pop(buffer,len);

		//Check rates
		switch (rate)
		{
			case 8000:
				//Calculate VAD
				vad += WebRtcVad_CalcVad8khz(&inst,buffer,len);
				break;
			case 16000:
				//Calculate VAD
				vad += WebRtcVad_CalcVad16khz(&inst,buffer,len);
				break;
			case 32000:
				//Calculate VAD
				vad += WebRtcVad_CalcVad32khz(&inst,buffer,len);
				break;
			case 48000:
				//Calculate VAD
				vad += WebRtcVad_CalcVad48khz(&inst,buffer,len);
				break;
			default:
				return 0;
		}
	}

	//No reate supported
	return vad;
}

bool VAD::SetMode(Mode mode)
{
	//Set mode
	return !WebRtcVad_set_mode_core(&inst,mode);
}

int VAD::GetVAD()
{
	return inst.vad;
}
#endif
