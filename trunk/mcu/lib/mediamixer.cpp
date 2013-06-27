#include "rtp.h"
#include "mp4recorder.h"
#include "audiomixer.h"
#include "audioencoder.h"
#include "audiodecoder.h"
#include "mp4player.h"

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

	uint8_t	MediaFrameGetMediaType(void* frame)
	{
		return ((MediaFrame*)frame)->GetType();
	}
	
	uint32_t MediaFrameGetDuration(void* frame)
	{
		return ((MediaFrame*)frame)->GetDuration();
	}


	uint8_t	RTPPacketGetMediaType(void* packet)
	{
		return ((RTPPacket*)packet)->GetMedia();
	}

	uint8_t	RTPPacketGetCodec(void* packet)
	{
		return ((RTPPacket*)packet)->GetCodec();
	}

	uint32_t RTPPacketGetTimestamp(void* packet)
	{
		return ((RTPPacket*)packet)->GetTimestamp();
	}

	uint8_t	RTPPacketGetMark(void* packet)
	{
		return ((RTPPacket*)packet)->GetMark();
	}

	uint8_t* RTPPacketGetData(void* packet)
	{
		return ((RTPPacket*)packet)->GetData();
	}

	uint32_t RTPPacketGetSize(void* packet)
	{
		return ((RTPPacket*)packet)->GetSize();
	}

	uint8_t* RTPPacketGetPayloadData(void* packet)
	{
		return ((RTPPacket*)packet)->GetMediaData();
	}

	uint32_t RTPPacketGetPayloadSize(void* packet)
	{
		return ((RTPPacket*)packet)->GetMediaLength();
	}

	void* MP4RecorderCreate()
	{
		 MP4Recorder* recorder = new MP4Recorder();
		 //Return
		 return (void*)recorder;
	}

	bool MP4RecorderRecord(void* recorder,const char* filename)
	{
		if (!((MP4Recorder*)recorder)->Create(filename))
                        return false;
                return ((MP4Recorder*)recorder)->Record();
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
		((MP4Recorder*)recorder)->Close();
		delete ((MP4Recorder*)recorder);
	}

	struct MP4PLayerListener {
		void (*onRTPPacket) (void*,void*);
		void (*onMediaFrame) (void*,void*);
		void (*onEnd) (void*);
	};
	
	class MP4PlayerWrapper : public MP4Streamer::Listener
	{
	public:
		MP4Streamer* streamer;
		MP4PLayerListener* listener;
		void *arg;
	public:
		virtual void onRTPPacket(RTPPacket &packet)
		{
			if (listener->onRTPPacket)
				listener->onRTPPacket(arg,&packet);
		}
		virtual void onTextFrame(TextFrame &text)
		{
		}
		virtual void onMediaFrame(MediaFrame &frame)
		{
			if (listener->onMediaFrame)
				listener->onMediaFrame(arg,&frame);
		}
		virtual void onEnd()
		{
			if (listener->onEnd)
				listener->onEnd(arg);
		}
	};

	void*	MP4PlayerCreate(MP4PLayerListener *listener,void* arg)
	{
		if (!listener)
			return NULL;

		MP4PlayerWrapper *wrapper = new MP4PlayerWrapper();
		wrapper->streamer = new MP4Streamer(wrapper);
		wrapper->listener = listener;
		wrapper->arg = arg;
		return wrapper;
	}

	bool MP4PlayerPlay(void* player,const char* filename)
	{
		MP4PlayerWrapper *wrapper = (MP4PlayerWrapper *)player;
		if (!wrapper->streamer->Open(filename))
			return false;
		if (!wrapper->streamer->Play())
			return false;
		return true;
	}

	void MP4PlayerStop(void* player)
	{
		MP4PlayerWrapper *wrapper = (MP4PlayerWrapper *)player;
		wrapper->streamer->Stop();
	}

	uint64_t MP4PlayerSeek(void* player,uint64_t seek)
	{
		MP4PlayerWrapper *wrapper = (MP4PlayerWrapper *)player;
		return wrapper->streamer->Seek(seek);
	}

	uint64_t MP4PlayerTell(void* player)
	{
		MP4PlayerWrapper *wrapper = (MP4PlayerWrapper *)player;
		return wrapper->streamer->Tell();
	}

	void MP4PlayerDestroy(void* player)
	{
		MP4PlayerWrapper *wrapper = (MP4PlayerWrapper *)player;
		delete wrapper->streamer;
		delete wrapper;
	}

	void*	AudioMixerCreate()
	{
		AudioMixer* mixer = new AudioMixer();
		mixer->Init(false);
		return mixer;
	}

	int	AudioMixerCreatePort(void* mixer,int id)
	{
		if (!((AudioMixer*)mixer)->CreateMixer(id))
			return 0;
		return ((AudioMixer*)mixer)->InitMixer(id,0);
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

