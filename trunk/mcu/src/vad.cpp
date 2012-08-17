#include "vad.h"

#ifdef VADWEBRTC
VAD::VAD()
{
	//Init webrtc vad
	WebRtcVad_InitCore(&inst);
	//Set agrresive mode
	WebRtcVad_set_mode_core(&inst,VERYAGGRESIVE);
}

int VAD::CalcVad8khz(SWORD* frame,DWORD size)
{
	//Calculate VAD
	return WebRtcVad_CalcVad8khz(&inst,frame,size);
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
