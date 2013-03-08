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
	bool AddListener(MediaFrame::Listener *listener);
	bool RemoveListener(MediaFrame::Listener *listener);
	int SetAudioCodec(AudioCodec::Type codec);
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
	typedef std::set<MediaFrame::Listener*> Listeners;
	
private:
	Listeners		listeners;
	AudioInput*		audioInput;
	AudioCodec::Type	audioCodec;
	pthread_mutex_t		mutex;
	pthread_t		encodingAudioThread;
	int			encodingAudio;
};

#endif	/* AUDIOENCODER_H */

