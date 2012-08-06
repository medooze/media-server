#include "../include/rtp.h"
#include "../include/mp4recorder.h"
#include "audiomixer.h"
#include "audioencoder.h"
#include "audiodecoder.h"

extern "C"
{

	void* RTPDepacketizerCreate(uint8_t mediaType,uint8_t codec)
	{
		return RTPDepacketizer::Create((MediaFrame::Type)mediaType,codec);
	}

	void* RTPDepacketizerAddPayload(void* rtp,uint8_t* payload,uint32_t payload_len)
	{
		return (void*)((RTPDepacketizer*)rtp)->AddPayload(payload,payload_len);
	}

	void RTPDepacketizerResetFrame(void* rtp)
	{
		((RTPDepacketizer*)rtp)->ResetFrame();
	}

	void RTPDepacketizerSetTimestamp(void* rtp,uint32_t timestamp)
	{
		((RTPDepacketizer*)rtp)->SetTimestamp(timestamp);
	}

	void RTPDepacketizerDelete(void* rtp)
	{
		delete (RTPDepacketizer*)rtp;
	}

	void* MP4RecorderCreate()
	{
		 MP4Recorder* recorder = new MP4Recorder();
		 //Init it
		 recorder->Init();
		 //Return
		 return (void*)recorder;
	}

	bool MP4RecorderRecord(void* recorder,const char* filename)
	{
		return ((MP4Recorder*)recorder)->Record(filename);
	}

	bool MP4RecorderStop(void* recorder)
	{
		return ((MP4Recorder*)recorder)->Stop();
	}
	void MP4RecorderOnMediaFrame(void* recorder,void* frame)
	{
		((MP4Recorder*)recorder)->onMediaFrame(*(MediaFrame*)frame);
	}

	void MP4RecorderDelete(void* recorder)
	{
		((MP4Recorder*)recorder)->End();
		delete ((MP4Recorder*)recorder);
	}
	void*	AudioMixerCreate()
	{
		AudioMixer* mixer = new AudioMixer();
		mixer->Init();
		return mixer;
	}

	int	AudioMixerCreatePort(void* mixer,int id)
	{
		if (!((AudioMixer*)mixer)->CreateMixer(id))
			return 0;
		return ((AudioMixer*)mixer)->InitMixer(id);
	}

	void	AudioMixerDeletePort(void* mixer,int id)
	{
		((AudioMixer*)mixer)->EndMixer(id);
		((AudioMixer*)mixer)->DeleteMixer(id);
	}

	void*	AudioMixerGetInput(void* mixer,int id)
	{
		return ((AudioMixer*)mixer)->GetInput(id);
	}

	void*	AudioMixerGetOutput(void* mixer,int id)
	{
		return ((AudioMixer*)mixer)->GetOutput(id);
	}

	void	AudioMixerDelete(void* mixer)
	{
		((AudioMixer*)mixer)->End();
		delete((AudioMixer*)mixer);
	}

	void*	AudioEncoderCreate(void* input,uint8_t codec)
	{
		AudioEncoder* enc = new AudioEncoder();
		enc->SetAudioCodec((AudioCodec::Type)codec);
		enc->Init((AudioInput*)input);
		enc->StartEncoding();
		return enc;
	}

	void	AudioEncoderAddRecorderListener(void* enc,void* recorder)
	{
		((AudioEncoder*)enc)->AddListener((MP4Recorder*)recorder);
	}

	void	AudioEncoderRemoveRecorederListener(void* enc,void* recorder)
	{
		((AudioEncoder*)enc)->RemoveListener((MP4Recorder*)recorder);
	}

	void	AudioEncoderDelete(void* enc)
	{
		((AudioEncoder*)enc)->StopEncoding();
		((AudioEncoder*)enc)->End();
		delete(((AudioEncoder*)enc));
	}

	void*	AudioDecoderCreate(void* output)
	{
		AudioDecoder* dec = new AudioDecoder();
		dec->Init((AudioOutput*)output);
		dec->Start();
		return  dec;

	}

	void	AudioDecoderOnRTPPacket(void* dec,uint8_t codec,uint32_t timestamp,uint8_t* data,uint32_t size)
	{
		RTPPacket pkt(MediaFrame::Audio,codec,codec);
		pkt.SetPayload(data,size);
		pkt.SetTimestamp(timestamp);
		((AudioDecoder*)dec)->onRTPPacket(pkt);
	}

	void	AudioDecoderDelete(void* dec)
	{
		((AudioDecoder*)dec)->Stop();
		((AudioDecoder*)dec)->End();
		delete(((AudioDecoder*)dec));
	}
}

