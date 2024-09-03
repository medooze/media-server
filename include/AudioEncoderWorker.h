#ifndef AUDIOENCODER_H_
#define	AUDIOENCODER_H_
#include "audio.h"
#include <set>

class AudioEncoderWorker
{
public:
	AudioEncoderWorker();
	~AudioEncoderWorker();

	int Init(AudioInput *input);
	bool AddListener(const MediaFrame::Listener::shared& listener);
	bool RemoveListener(const MediaFrame::Listener::shared& listener);
	int SetAudioCodec(AudioCodec::Type codec, const Properties& properties);
	int StartEncoding();
	int StopEncoding();
	int End();

	int IsEncoding() { return encodingAudio;}

protected:
	int Encode();


private:
	//Funciones propias
	static void *startEncoding(void *par);
	AudioEncoder* CreateAudioEncoder(AudioCodec::Type type);

private:
	typedef std::set<MediaFrame::Listener::shared> Listeners;
	
private:
	Listeners		listeners;
	AudioInput*		audioInput = nullptr;
	AudioCodec::Type	audioCodec = AudioCodec::PCMU;
	Properties		audioProperties;
	pthread_mutex_t		mutex;
	pthread_t		encodingAudioThread = 0;
	int			encodingAudio = 0;
};

#endif	/* AUDIOENCODER_H */

