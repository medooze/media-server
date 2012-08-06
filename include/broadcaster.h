#ifndef _BROADCASTER_H_
#define _BROADCASTER_H_
#include <map>
#include <string>
#include "pthread.h"
#include "broadcastsession.h"
#include "rtmpstream.h"
#include "rtmpapplication.h"

class Broadcaster : 
	public RTMPApplication,
	public RTMPNetConnection
{
public:
	class NetStream : public RTMPNetStream
	{
	public:
		NetStream(DWORD id,Listener *listener);
		virtual ~NetStream();

		//Netstream API
		virtual void doPlay(std::wstring& url,RTMPMediaStream::Listener *listener);
		virtual void doPublish(std::wstring& url);
		virtual void doPause();
		virtual void doResume();
		virtual void doSeek(DWORD time);
		virtual void doClose(RTMPMediaStream::Listener *listener);
	};
public:
	Broadcaster();
	~Broadcaster();

	bool Init();
	bool End();

	DWORD CreateBroadcast(const std::wstring &name,const std::wstring &tag);
	bool  PublishBroadcast(DWORD id,const std::wstring &pin);
	bool  AddBroadcastToken(DWORD id,const std::wstring &token);
	bool  UnPublishBroadcast(DWORD id);
	bool  GetBroadcastRef(DWORD id,BroadcastSession **broadcast);
	bool  ReleaseBroadcastRef(DWORD id);
	bool  DeleteBroadcast(DWORD confId);
	bool  GetBroadcastPublishedStreams(DWORD id,BroadcastSession::PublishedStreamsInfo &list);

	/** RTMP application interface*/
	virtual RTMPNetConnection* Connect(const std::wstring& appName,RTMPNetConnection::Listener* listener);

	/* For RTMPNetConnection */
	virtual RTMPNetStream* CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener* listener);
	virtual void DeleteStream(RTMPNetStream *stream);
protected:
	DWORD GetPublishedBroadcastId(const std::wstring &pin);
	DWORD GetTokenBroadcastId(const std::wstring &token);
protected:

	struct BroadcastEntry
	{
		DWORD id;
		DWORD numRef;
		DWORD enabled;
		std::wstring name;
                std::wstring tag;
		std::wstring pin;
		BroadcastSession* session;
	};
	typedef std::map<DWORD,BroadcastEntry> BroadcastEntries;
	typedef std::map<std::wstring,DWORD> PublishedBroadcasts;
	typedef std::map<std::wstring,DWORD> BroadcastTokens;

private:
	BroadcastEntries	broadcasts;
	PublishedBroadcasts	published;
	BroadcastTokens		tokens;
	DWORD			maxId;
	pthread_mutex_t		mutex;
	bool inited;
};

#endif
