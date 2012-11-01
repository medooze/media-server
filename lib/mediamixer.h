/* 
 * File:   mediamixer.h
 * Author: Sergio
 *
 * Created on 12 de julio de 2012, 15:54
 */

#ifndef MEDIAMIXER_H
#define	MEDIAMIXER_H
#ifdef	__cplusplus
extern "C"
{
#endif

#define MEDIA_FRAME_TYPE_AUDIO 0
#define MEDIA_FRAME_TYPE_VIDEO 1
#define MEDIA_VIDEO_CODEC_H263_1996	34
#define MEDIA_VIDEO_CODEC_H263_1998	103
#define MEDIA_VIDEO_CODEC_MPEG4		104
#define MEDIA_VIDEO_CODEC_H264		99
#define MEDIA_VIDEO_CODEC_SORENSON	100
#define MEDIA_AUDIO_CODEC_PCMA		8
#define MEDIA_AUDIO_CODEC_PCMU		0
#define MEDIA_AUDIO_CODEC_GSM		3
#define MEDIA_AUDIO_CODEC_SPEEX16	117
#define MEDIA_AUDIO_CODEC_AMR		118
#define MEDIA_AUDIO_CODEC_NELLY8		130
#define MEDIA_AUDIO_CODEC_NELLY11	131
typedef void* AudioInput;
typedef void* AudioOutput;
typedef void* AudioMixer;
typedef void* AudioEncoder;
typedef void* AudioDecoder;
typedef void* RTPDepacketizer;
typedef void* MediaFrame;
typedef void* MP4Recorder;
typedef void* MP4Player;
typedef void* RTPPacket;
typedef struct
{
	void (*onRTPPacket) (void*,RTPPacket);
	void (*onMediaFrame) (void*,MediaFrame);
	void (*onEnd) (void*);
} MP4PLayerListener;

RTPDepacketizer RTPDepacketizerCreate(uint8_t mediaType,uint8_t codec);
void		RTPDepacketizerSetTimestamp(void* rtp,uint32_t timestamp);
MediaFrame	RTPDepacketizerAddPayload(RTPDepacketizer rtp,uint8_t* payload,uint32_t payload_len);
void		RTPDepacketizerResetFrame(RTPDepacketizer rtp);
void		RTPDepacketizerDelete(RTPDepacketizer rtp);

uint8_t		MediaFrameGetMediaType(MediaFrame frame);
uint32_t	MediaFrameGetDuration(MediaFrame frame);

uint8_t		RTPPacketGetMediaType(RTPPacket packet);
uint8_t		RTPPacketGetCodec(RTPPacket packet);
uint32_t	RTPPacketGetTimestamp(RTPPacket packet);
uint8_t		RTPPacketGetMark(RTPPacket packet);
uint8_t*	RTPPacketGetData(RTPPacket packet);
uint32_t	RTPPacketGetSize(RTPPacket packet);
uint8_t*	RTPPacketGetPayloadData(RTPPacket packet);
uint32_t	RTPPacketGetPayloadSize(RTPPacket packet);

MP4Recorder	MP4RecorderCreate();
int		MP4RecorderRecord(MP4Recorder recorder,const char* filename);
int		MP4RecorderStop(MP4Recorder recorder);
void		MP4RecorderOnMediaFrame(MP4Recorder recorder,MediaFrame frame);
void		MP4RecorderDelete(MP4Recorder recorder);

MP4Player	MP4PlayerCreate(MP4PLayerListener* listener, void *arg);
int		MP4PlayerPlay(MP4Player player,const char* filename);
void		MP4PlayerStop(MP4Player player);
uint64_t	MP4PlayerSeek(MP4Player player, uint64_t seek);
uint64_t	MP4PlayerTell(MP4Player player);
void		MP4PlayerDestroy(MP4Player player);

AudioMixer	AudioMixerCreate();
int		AudioMixerCreatePort(AudioMixer mixer,int id);
void		AudioMixerDeletePort(AudioMixer mixer,int id);
AudioInput	AudioMixerGetInput(AudioMixer mixer,int id);
AudioOutput	AudioMixerGetOutput(AudioMixer mixer,int id);
void		AudioMixerDelete(AudioMixer mixer);

AudioEncoder	AudioEncoderCreate(AudioInput input,uint8_t codec);
void		AudioEncoderAddRecorderListener(AudioEncoder enc,MP4Recorder recorder);
void		AudioEncoderRemoveListener(AudioEncoder enc,MP4Recorder recorder);
void		AudioEncoderDelete(AudioEncoder enc);

AudioDecoder	AudioDecoderCreate(AudioOutput output);
void		AudioDecoderOnRTPPacket(AudioDecoder dec,uint8_t codec,uint32_t timestamp,uint8_t* payload,uint32_t payload_len);
void		AudioDecoderDelete(AudioDecoder dec);

#ifdef	__cplusplus
}
#endif
#endif	/* MEDIAMIXER_H */

