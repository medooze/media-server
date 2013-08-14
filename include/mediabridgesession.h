/* 
 * File:   mediabridgesession.h
 * Author: Sergio
 *
 * Created on 22 de diciembre de 2010, 18:20
 */

#ifndef _MEDIABRIDGESESSION_H_
#define	_MEDIABRIDGESESSION_H_
#include "rtpsession.h"
#include "rtmpnetconnection.h"
#include "rtmpstream.h"
#include "codecs.h"
#include "mp4recorder.h"
#include "waitqueue.h"
#include "RTPSmoother.h"
#include <set>

class MediaBridgeSession  :
	public RTMPNetConnection,
	public RTMPMediaStream,
	public RTMPMediaStream::Listener
{
public:
	class NetStream : public RTMPNetStream
	{
	public:
		NetStream(DWORD streamId,MediaBridgeSession *sess,RTMPNetStream::Listener *listener);
		virtual ~NetStream();
		virtual void doPlay(std::wstring& url,RTMPMediaStream::Listener *listener);
		virtual void doPublish(std::wstring& url);
		virtual void doClose(RTMPMediaStream::Listener *listener);
	private:
		void Close();
	private:
		MediaBridgeSession *sess;
	};
public:
	MediaBridgeSession();
	~MediaBridgeSession();

	bool Init();

	//Video RTP
	int StartSendingVideo(char *sendVideoIp,int sendVideoPort,RTPMap& rtpMap);
	int SetSendingVideoCodec(VideoCodec::Type codec);
	int SendFPU();
	int StopSendingVideo();
	int StartReceivingVideo(RTPMap& rtpMap);
	int StopReceivingVideo();
	//Audio RTP
	int StartSendingAudio(char *sendAudioIp,int sendAudioPort,RTPMap& rtpMap);
	int SetSendingAudioCodec(AudioCodec::Type codec);
	int StopSendingAudio();
	int StartReceivingAudio(RTPMap& rtpMap);
	int StopReceivingAudio();
	//T140 Text RTP
	int StartSendingText(char *sendTextIp,int sendTextPort,RTPMap& rtpMap);
	int StopSendingText();
	int StartReceivingText(RTPMap& rtpMap);
	int StopReceivingText();
	int SetSendingTextCodec(TextCodec::Type codec);

	bool End();

	void AddInputToken(const std::wstring &token);
	void AddOutputToken(const std::wstring &token);

	bool ConsumeOutputToken(const std::wstring &token);
	bool ConsumeInputToken(const std::wstring &token);

	/** RTMPNetConnection */
	//virtual void Connect(RTMPNetConnection::Listener* listener); -> Not needed to be overriden yet
	virtual RTMPNetStream* CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener* listener);
	virtual void DeleteStream(RTMPNetStream *stream);
	//virtual void Disconnect(RTMPNetConnection::Listener* listener);  -> Not needed to be overriden yet

	/* Overrride from RTMPMediaStream*/
	virtual DWORD AddMediaListener(RTMPMediaStream::Listener *listener);
	virtual DWORD RemoveMediaListener(RTMPMediaStream::Listener *listener);

	//RTMPMediaStream Listener
	virtual void onAttached(RTMPMediaStream *stream);
	virtual void onMediaFrame(DWORD id,RTMPMediaFrame *frame);
	virtual void onMetaData(DWORD id,RTMPMetaData *meta);
	virtual void onCommand(DWORD id,const wchar_t *name,AMFData* obj);
	virtual void onStreamBegin(DWORD id);
	virtual void onStreamEnd(DWORD id);
	virtual void onStreamReset(DWORD id);
	virtual void onDetached(RTMPMediaStream *stream);
	
protected:
	int RecVideo();
	int RecAudio();
	int RecText();

	int SendVideo();
	int SendAudio();

	int DecodeAudio();

private:
	static void* startReceivingText(void *par);
	static void* startReceivingVideo(void *par);
	static void* startReceivingAudio(void *par);

	static void* startSendingVideo(void *par);
	static void* startSendingAudio(void *par);
	static void* startDecodingAudio(void *par);

private:
	typedef std::set<std::wstring> ParticipantTokens;

private:
	//Tokens
	ParticipantTokens inputTokens;
	ParticipantTokens outputTokens;
	//Bridged sessions
	RTMPMetaData 	*meta;
	RTPSession      rtpAudio;
	RTPSession      rtpVideo;
	RTPSession      rtpText;
	RTPSmoother	smoother;

	WaitQueue<RTMPVideoFrame*> videoFrames;
	WaitQueue<RTMPAudioFrame*> audioFrames;
	
	VideoCodec::Type rtpVideoCodec;
	AudioCodec::Type rtpAudioCodec;
	TextCodec::Type  rtpTextCodec;

	AudioEncoder *rtpAudioEncoder;
	AudioDecoder *rtpAudioDecoder;
	AudioEncoder *rtmpAudioEncoder;
	AudioDecoder *rtmpAudioDecoder;

	//Las threads
	pthread_t 	recVideoThread;
	pthread_t 	recAudioThread;
	pthread_t 	recTextThread;
	pthread_t 	sendVideoThread;
	pthread_t 	sendAudioThread;
	pthread_t	decodeAudioThread;
	pthread_mutex_t	mutex;

	//Controlamos si estamos mandando o no
	bool	sendingVideo;
	bool 	receivingVideo;
	bool	sendingAudio;
	bool	receivingAudio;
	bool	sendingText;
	bool 	receivingText;
	bool	inited;
	bool	waitVideo;
	bool	sendFPU;
	timeval	first;
};

#endif	/* MEDIABRIDGESESSION_H */

