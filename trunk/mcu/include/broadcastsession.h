#ifndef _BROADCASTSESSION_H_
#define _BROADCASTSESSION_H_
#include "config.h"
#include "rtmpnetconnection.h"
#include "rtmpclientconnection.h"
#include "rtmpmessage.h"
#include "rtmpstream.h"
#include "flvrecorder.h"
#include <set>
#include <map>


class BroadcastSession : public RTMPClientConnection::Listener
{
public:
	struct StreamInfo
	{
		std::wstring name;
		std::wstring url;
		std::string ip;
	};
	typedef std::map<std::wstring,StreamInfo> PublishedStreamsInfo;
private:
	class PublisherNetStream : public RTMPNetStream
	{
	public:
		PublisherNetStream(DWORD id,BroadcastSession *sess,Listener *listener);
		virtual ~PublisherNetStream();
		virtual void doPublish(std::wstring& url);
		virtual void doClose(RTMPMediaStream::Listener *listener);
		virtual void onDetached(RTMPMediaStream* stream);
	private:
		void Close();
	private:
		BroadcastSession *sess;
		RTMPPipedMediaStream* stream;
	};
	
	class WatcherNetStream : public RTMPNetStream
	{
	public:
		WatcherNetStream(DWORD id,BroadcastSession *sess,Listener *listener);
		virtual ~WatcherNetStream();
		virtual void doPlay(std::wstring& url,RTMPMediaStream::Listener *listener);
		virtual void doClose(RTMPMediaStream::Listener *listener);
		virtual void onDetached(RTMPMediaStream* stream);
	private:
		void Close();
	private:
		BroadcastSession *sess;
		RTMPPipedMediaStream* stream;
	};

	class NetConnection : public RTMPNetConnection
	{
	public:
		enum Type { Publisher,Watcher};
	public:
		NetConnection(Type type,BroadcastSession *sess);
		Type GetType();
		/** RTMPNetConnection */
		//virtual void Connect(RTMPNetConnection::Listener* listener);
		virtual RTMPNetStream* CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener* listener);
		virtual void DeleteStream(RTMPNetStream *stream);
		virtual void Disconnect(RTMPNetConnection::Listener* listener);
	private:
		Type type;
		BroadcastSession *sess;
	};
	
public:
	BroadcastSession(const std::wstring &tag);
	~BroadcastSession();
	bool Init(DWORD maxTransfer,DWORD maxConcurrent);
	RTMPNetConnection* ConnectPublisher(RTMPNetConnection::Listener* listener);
	RTMPNetConnection* ConnectWatcher(RTMPNetConnection::Listener* listener);
	void onDisconnected(NetConnection* connection);
	bool End();

	bool  GetBroadcastPublishedStreams(BroadcastSession::PublishedStreamsInfo &list);
	
	/** RTMPClient listener for re transmitters*/
	void onConnected(RTMPClientConnection* conn);
	void onNetStreamCreated(RTMPClientConnection* conn,RTMPClientConnection::NetStream *stream);
	void onCommandResponse(RTMPClientConnection* conn,DWORD id,bool isError,AMFData* param);
	void onDisconnected(RTMPClientConnection* conn);
protected:
	RTMPPipedMediaStream* PlayMediaStream(const std::wstring &name,WatcherNetStream *netStream);
	RTMPPipedMediaStream* PublishMediaStream(const std::wstring &name,PublisherNetStream *netStream);
private:
	typedef std::set<RTMPNetConnection*>	Publishers;
	typedef std::set<RTMPNetConnection*>	Watchers;
	typedef std::set<RTMPClientConnection*>	Transmitters;
	typedef std::map<std::wstring,RTMPPipedMediaStream*>	MediaSreams;
private:
	MediaSreams	streams;
	Publishers	publishers;
	Watchers	watchers;
	Transmitters	transmitters;
	std::wstring	tag;
	
	bool		inited;
	DWORD		sent;
	DWORD		maxTransfer;
	DWORD		maxConcurrent;
	Use		lock;
};

#endif
