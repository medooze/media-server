#include "vad.h"

#ifdef VADWEBRTC
VAD::VAD()
{
	//Init webrtc vad
	WebRtcVad_InitCore(&inst);
	//Set agrresive mode
	WebRtcVad_set_mode_core(&inst,VERYAGGRESIVE);
}

int VAD::CalcVad(SWORD* frame,DWORD size,DWORD rate)
{
	//Check rates
	switch (rate)
	{
		case 8000:
			//Calculate VAD
			return WebRtcVad_CalcVad8khz(&inst,frame,size);
		case 16000:
			//Calculate VAD
			return WebRtcVad_CalcVad16khz(&inst,frame,size);
		case 32000:
			//Calculate VAD
			return WebRtcVad_CalcVad32khz(&inst,frame,size);
		case 48000:
			/Calculate VAD
			return WebRtcVad_CalcVad48khz(&inst,frame,size);
	}

	//No reate supported
	return 0;
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
