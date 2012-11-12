#ifndef _MULTICONF_H_
#define _MULTICONF_H_
#include "videomixer.h"
#include "audiomixer.h"
#include "textmixer.h"
#include "videomixer.h"
#include "participant.h"
#include "FLVEncoder.h"
#include "broadcastsession.h"
#include "mp4player.h"
#include "mp4recorder.h"
#include "audioencoder.h"
#include "textencoder.h"
#include "rtmpnetconnection.h"
#include <map>
#include <string>

class MultiConf :
	public RTMPNetConnection,
	public Participant::Listener
{
public:
	typedef std::map<std::string,MediaStatistics> ParticipantStatistics;
public:
	class NetStream : public RTMPNetStream
	{
	public:
		NetStream(DWORD streamId,MultiConf *conf,RTMPNetStream::Listener *listener);
		virtual ~NetStream();
		virtual void doPlay(std::wstring& url,RTMPMediaStream::Listener *listener);
		virtual void doPublish(std::wstring& url);
		virtual void doSeek(DWORD time);
		virtual void doClose(RTMPMediaStream::Listener *listener);
	protected:
		void Close();
	private:
		MultiConf *conf;
		bool opened;
	};

	class Listener
	{
	public:
		virtual void onParticipantRequestFPU(MultiConf *conf,int partId,void *param) = 0;
	};
	
public:
	MultiConf(const std::wstring& tag);
	~MultiConf();

	int Init(bool isVADenabled);
	int End();

	void SetListener(Listener *listener,void* param);

	int CreateMosaic(Mosaic::Type comp,int size);
	int SetMosaicOverlayImage(int mosaicId,const char* filename);
	int ResetMosaicOverlay(int mosaicId);
	int DeleteMosaic(int mosaicId);
	int CreateSidebar();
	int DeleteSidebar(int sidebarId);
	int CreateParticipant(int mosaicId,int sidebarId,std::wstring name,Participant::Type type);
	int StartRecordingParticipant(int partId,const char* filename);
	int StopRecordingParticipant(int partId);
	int SendFPU(int partId);
	int SetMute(int partId,MediaFrame::Type media,bool isMuted);
	ParticipantStatistics* GetParticipantStatistic(int partId);
	int SetParticipantMosaic(int partId,int mosaicId);
	int SetParticipantSidebar(int partId,int sidebarId);
	int DeleteParticipant(int partId);

	int CreatePlayer(int privateId,std::wstring name);
	int StartPlaying(int playerId,const char* filename,bool loop);
	int StopPlaying(int playerId);
	int DeletePlayer(int playerId);

	int SetCompositionType(int mosaicId,Mosaic::Type comp,int size);
	int SetMosaicSlot(int mosaicId,int num,int id);
	int AddMosaicParticipant(int mosaicId,int partId);
	int RemoveMosaicParticipant(int mosaicId,int partId);
	int AddSidebarParticipant(int sidebar,int partId);
	int RemoveSidebarParticipant(int sidebar,int partId);
	
	int StartSending(int partId,MediaFrame::Type media,char *sendIp,int sendPort,RTPMap& rtpMap);
	int StopSending(int partId,MediaFrame::Type media);
	int StartReceiving(int partId,MediaFrame::Type media,RTPMap& rtpMap);
	int StopReceiving(int partId,MediaFrame::Type media);
	int SetLocalCryptoSDES(int id,MediaFrame::Type media,const char *suite,const char* key);
	int SetRemoteCryptoSDES(int id,MediaFrame::Type media,const char *suite,const char* key);
	int SetLocalSTUNCredentials(int id,MediaFrame::Type media,const char *username,const char* pwd);

	int SetVideoCodec(int partId,int codec,int mode,int fps,int bitrate,int quality=0, int fillLevel=0,int intraPeriod = 0);
	int SetAudioCodec(int partId,int codec);
	int SetTextCodec(int partId,int codec);

	int  StartBroadcaster();
	int  StopBroadcaster();

	bool AddParticipantInputToken(int partId,const std::wstring &token);
	bool AddParticipantOutputToken(int partId,const std::wstring &token);
	bool AddBroadcastToken(const std::wstring &token);

	RTMPMediaStream* ConsumeParticipantOutputToken(const std::wstring &token);
	RTMPMediaStream::Listener* ConsumeParticipantInputToken(const std::wstring &token);
	RTMPMediaStream* ConsumeBroadcastToken(const std::wstring &token);

	int GetNumParticipants() { return participants.size(); }
	std::wstring& GetTag() { return tag;	}

	/** Participants event */
	void onRequestFPU(Participant *part);

	/** RTMPNetConnection */
	//virtual void Connect(RTMPNetConnection::Listener* listener); -> Not needed to be overriden yet
	virtual RTMPNetStream* CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener* listener);
	virtual void DeleteStream(RTMPNetStream *stream);
	//virtual void Disconnect(RTMPNetConnection::Listener* listener);  -> Not needed to be overriden yet
private:
	Participant *GetParticipant(int partId);
	Participant *GetParticipant(int partId,Participant::Type type);
	void DestroyParticipant(int partId,Participant* part);
private:
	typedef std::map<int,Participant*> Participants;
	typedef std::set<std::wstring> BroadcastTokens;
	typedef std::map<std::wstring,DWORD> ParticipantTokens;
	typedef std::map<int, MP4Player*> Players;
	
private:
	ParticipantTokens	inputTokens;
	ParticipantTokens	outputTokens;
	BroadcastTokens		tokens;
	//Atributos
	int		inited;
	int		maxId;
	std::wstring	tag;

	Listener *listener;
	void* param;

	//Los mixers
	VideoMixer videoMixer;
	AudioMixer audioMixer;
	TextMixer  textMixer;

	//Lists
	Participants		participants;
	Players			players;

	int			watcherId;
	int			broadcastId;
	FLVEncoder		flvEncoder;
	AudioEncoder		audioEncoder;
	TextEncoder		textEncoder;
	BroadcastSession	broadcast;
	FLVRecorder		recorder;

	Use			participantsLock;
};

#endif
