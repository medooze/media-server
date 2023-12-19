#ifndef _RTMPSTREAM_H_
#define _RTMPSTREAM_H_
#include "config.h"
#include "use.h"
#include "rtmpmessage.h"
#include "waitqueue.h"
#include <string>
#include <list>
#include <set>
#include <memory>

struct RTMPNetStatusEventInfo
{
	const wchar_t* code;
	const wchar_t* level;

	bool operator ==(const wchar_t* code) const
	{
		return wcscmp(this->code,code)==0;
	}
	bool operator !=(const wchar_t* code) const
	{
		return wcscmp(this->code,code)!=0;
	}

	bool operator ==(const std::wstring& code) const
	{
		return code.compare(this->code)==0;
	}
	bool operator !=(const std::wstring& code) const
	{
		return code.compare(this->code)!=0;
	}
};

namespace RTMP
{
	namespace NetConnection
	{
		namespace Call
		{
			const RTMPNetStatusEventInfo BadVersion		= {L"NetConnection.Call.BadVersion"		,L"error"};	//Packet encoded in an unidentified format.
			const RTMPNetStatusEventInfo Failed		= {L"NetConnection.Call.Failed"			,L"error"};	//The NetConnection.call() method was not able to invoke the server-side method or command.
			const RTMPNetStatusEventInfo Prohibited		= {L"NetConnection.Call.Prohibited"		,L"error"};	//An Action Message Format (AMF) operation is prevented for security reasons. Either the AMF URL is not in the same domain as the file containing the code calling the NetConnection.call() method, or the AMF server does not have a policy file that trusts the domain of the the file containing the code calling the NetConnection.call() method.
		}
		namespace Connect
		{
			const RTMPNetStatusEventInfo AppShutdown	= {L"NetConnection.Connect.AppShutdown"		,L"error"};	//The server-side application is shutting down.
			const RTMPNetStatusEventInfo Closed		= {L"NetConnection.Connect.Closed"		,L"status"};	//The connection was closed successfully.
			const RTMPNetStatusEventInfo Failed		= {L"NetConnection.Connect.Failed"		,L"error"};	//The connection attempt failed.
			const RTMPNetStatusEventInfo IdleTimeout	= {L"NetConnection.Connect.IdleTimeout"		,L"status"};	//Flash Media Server disconnected the client because the client was idle longer than the configured value for <MaxIdleTime>. on Flash Media Server, <AutoCloseIdleClients> is disabled by default. When enabled, the default timeout value is 3600 seconds (1 hour). For more information, see Close idle connections.
			const RTMPNetStatusEventInfo InvalidApp		= {L"NetConnection.Connect.InvalidApp"		,L"error"};	//The application name specified in the call to NetConnection.connect() is invalid.
			const RTMPNetStatusEventInfo NetworkChange	= {L"NetConnection.Connect.NetworkChange"	,L"status"};	//Flash Player has detected a network change, for example, a dropped wireless connection, a successful wireless connection,or a network cable loss.Use this event to check for a network interface change. Don't use this event to implement your NetConnection reconnect logic. Use "NetConnection.Connect.Closed" to implement your NetConnection reconnect logic.
			const RTMPNetStatusEventInfo Rejected		= {L"NetConnection.Connect.Rejected"		,L"error"};	//The connection attempt did not have permission to access the application.
		}
	}
	namespace Netstream
	{
		const RTMPNetStatusEventInfo Failed			= {L"NetStream.Failed"	,L"error"};	//(Flash Media Server) An error has occurred for a reason other than those listed in other event codes.

		namespace Buffer
		{
			const RTMPNetStatusEventInfo Empty		= {L"NetStream.Buffer.Empty"			,L"status"};	//Flash Player is not receiving data quickly enough to fill the buffer. Data flow is interrupted until the buffer refills, at which time a NetStream.Buffer.Full message is sent and the stream begins playing again.
			const RTMPNetStatusEventInfo Flush		= {L"NetStream.Buffer.Flush"			,L"status"};	//Data has finished streaming, and the remaining buffer is emptied. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo Full		= {L"NetStream.Buffer.Full"			,L"status"};	//The buffer is full and the stream begins playing.
		}
		namespace Connect
		{
			const RTMPNetStatusEventInfo Closed		= {L"NetStream.Connect.Closed"			,L"status"};	//The P2P connection was closed successfully. The info.stream property indicates which stream has closed. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo Failed		= {L"NetStream.Connect.Failed"			,L"error"};	//The P2P connection attempt failed. The info.stream property indicates which stream has failed. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo Rejected		= {L"NetStream.Connect.Rejected"		,L"error"};	//The P2P connection attempt did not have permission to access the other peer. The info.stream property indicates which stream was rejected. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo Sucess		= {L"NetStream.Connect.Success"			,L"status"};	//The P2P connection attempt succeeded. The info.stream property indicates which stream has succeeded. Note: Not supported in AIR 3.0 for iOS.
		}
		namespace DRM
		{
			const RTMPNetStatusEventInfo UpdateNeeded	= {L"NetStream.DRM.UpdateNeeded"		,L"status"};	//A NetStream object is attempting to play protected content, but the required Flash Access module is either not present, not permitted by the effective content policy, or not compatible with the current player. To update the module or player, use the update() method of flash.system.SystemUpdater. Note: Not supported in AIR 3.0 for iOS.
		}
		namespace MulticasStream
		{
			const RTMPNetStatusEventInfo Reset		= {L"NetStream.MulticastStream.Reset"		,L"status"};	//A multicast subscription has changed focus to a different stream published with the same name in the same group. Local overrides of multicast stream parameters are lost. Reapply the local overrides or the new stream's default parameters will be used.

		}
		namespace Pause
		{
			const RTMPNetStatusEventInfo Notify		= {L"NetStream.Pause.Notify"			,L"status"};	//The stream is paused.

		}
		namespace Play
		{
			const RTMPNetStatusEventInfo Failed			= {L"NetStream.Play.Failed"			,L"error"};	//An error has occurred in playback for a reason other than those listed elsewhere in this table, such as the subscriber not having read access. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo FileStructureInvalid	= {L"NetStream.Play.FileStructureInvalid"	,L"error"};	//(AIR and Flash Player 9.0.115.0) The application detects an invalid file structure and will not try to play this type of file. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo InsufficientBW		= {L"NetStream.Play.InsufficientBW"		,L"warning"};	//(Flash Media Server) The client does not have sufficient bandwidth to play the data at normal speed. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo NoSupportedTrackFound	= {L"NetStream.Play.NoSupportedTrackFound"	,L"status"};	//(AIR and Flash Player 9.0.115.0) The application does not detect any supported tracks (video, audio or data) and will not try to play the file. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo PublishNotify		= {L"NetStream.Play.PublishNotify"		,L"status"};	//The initial publish to a stream is sent to all subscribers.
			const RTMPNetStatusEventInfo Reset			= {L"NetStream.Play.Reset"			,L"status"};	//Caused by a play list reset. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo Start			= {L"NetStream.Play.Start"			,L"status"};	//Playback has started.
			const RTMPNetStatusEventInfo Stop			= {L"NetStream.Play.Stop"			,L"status"};	//Playback has stopped.
			const RTMPNetStatusEventInfo StreamNotFound		= {L"NetStream.Play.StreamNotFound"		,L"error"};	//The file passed to the NetStream.play() method can't be found.
			const RTMPNetStatusEventInfo Transition			= {L"NetStream.Play.Transition"			,L"status"};	//(Flash Media Server 3.5) The server received the command to transition to another stream as a result of bitrate stream switching. This code indicates a success status event for the NetStream.play2() call to initiate a stream switch. If the switch does not succeed, the server sends a NetStream.Play.Failed event instead. When the stream switch occurs, an onPlayStatus event with a code of "NetStream.Play.TransitionComplete" is dispatched. For Flash Player 10 and later. Note: Not supported in AIR 3.0 for iOS.
			const RTMPNetStatusEventInfo UnpublishNotify		= {L"NetStream.Play.UnpublishNotify"		,L"status"};	//An unpublish from a stream is sent to all subscribers.

		}
		namespace Publish
		{
			const RTMPNetStatusEventInfo BadName		= {L"NetStream.Publish.BadName"			,L"error"};	//Attempt to publish a stream which is already being published by someone else.
			const RTMPNetStatusEventInfo Idle		= {L"NetStream.Publish.Idle"			,L"status"};	//The publisher of the stream is idle and not transmitting data.
			const RTMPNetStatusEventInfo Start		= {L"NetStream.Publish.Start"			,L"status"};	//Publish was successful.
			const RTMPNetStatusEventInfo Rejected		= {L"NetStream.Publish.Rejected"		,L"status"};	
			const RTMPNetStatusEventInfo Denied		= {L"NetStream.Publish.Denied"			,L"status"};	

		}
		namespace Record
		{
			const RTMPNetStatusEventInfo AlreadyExists	= {L"NetStream.Record.AlreadyExists"		,L"status"};	//The stream being recorded maps to a file that is already being recorded to by another stream. This can happen due to misconfigured virtual directories.
			const RTMPNetStatusEventInfo Failed		= {L"NetStream.Record.Failed"			,L"error"};	//An attempt to record a stream failed.
			const RTMPNetStatusEventInfo NoAccess		= {L"NetStream.Record.NoAccess"			,L"error"};	//Attempt to record a stream that is still playing or the client has no access right.
			const RTMPNetStatusEventInfo Start		= {L"NetStream.Record.Start"			,L"status"};	//Recording has started.
			const RTMPNetStatusEventInfo Stop		= {L"NetStream.Record.Stop"			,L"status"};	//Recording stopped.

		}
		namespace Seek
		{
			const RTMPNetStatusEventInfo Failed		= {L"NetStream.Seek.Failed"			,L"error"};	//The seek fails, which happens if the stream is not seekable.
			const RTMPNetStatusEventInfo InvalidTime	= {L"NetStream.Seek.InvalidTime"		,L"error"};	//For video downloaded progressively, the user has tried to seek or play past the end of the video data that has downloaded thus far, or past the end of the video once the entire file has downloaded. The info.details property of the event object contains a time code that indicates the last valid position to which the user can seek.
			const RTMPNetStatusEventInfo Notify		= {L"NetStream.Seek.Notify"			,L"status"};	//The seek operation is complete. Sent when NetStream.seek() is called on a stream in AS3 NetStream Data Generation Mode. The info object is extended to include info.seekPoint which is the same value passed to NetStream.seek().

		}
		namespace Unpublish
		{
			const RTMPNetStatusEventInfo Success		= {L"NetStream.Unpublish.Success"		,L"status"};	//The unpublish operation was successfuul.

		}
		namespace Unpause
		{
			const RTMPNetStatusEventInfo Notify		= {L"NetStream.Unpause.Notify"			,L"status"};	//The stream is resumed.
		}
		namespace Step
		{
			const RTMPNetStatusEventInfo Notify		= {L"NetStream.Step.Notify"			,L"status"};	//The step operation is complete. Note: Not supported in AIR 3.0 for iOS.
		}
	}
}

class RTMPMediaStream
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onAttached(RTMPMediaStream *stream) = 0;
		virtual void onMediaFrame(DWORD id,RTMPMediaFrame *frame) = 0;
		virtual void onMetaData(DWORD id,RTMPMetaData *meta) = 0;
		virtual void onCommand(DWORD id,const wchar_t *name,AMFData* obj) = 0;
		virtual void onStreamBegin(DWORD id) = 0;
		virtual void onStreamEnd(DWORD id) = 0;
		//virtual void onStreamIsRecorded(DWORD id) = 0;
		virtual void onStreamReset(DWORD id) = 0;
		virtual void onDetached(RTMPMediaStream *stream)  = 0;
	};
public:
	RTMPMediaStream();
	RTMPMediaStream(DWORD id);
	virtual ~RTMPMediaStream();
	virtual DWORD AddMediaListener(Listener *listener);
	virtual DWORD RemoveMediaListener(Listener *listener);
	virtual void RemoveAllMediaListeners();
	DWORD GetNumListeners()		{ return listeners.size(); }
	DWORD GetStreamId()		{ return id;		}
	void  SetData(DWORD data)	{ this->data = data;	}
	DWORD GetData()			{ return data;		}
	void  SetTag(const std::wstring &tag)	{ this->tag = tag;	}
	std::wstring GetTag()		{ return tag;		}
	void SetRTT(DWORD rtt)		{ this->rtt = rtt;	}
	DWORD GetRTT() const		{ return rtt;		}

	virtual void SendMetaData(RTMPMetaData *meta);
	virtual void SendMediaFrame(RTMPMediaFrame *frame);
	virtual void SendCommand(const wchar_t *name,AMFData* obj);
	virtual void SendStreamBegin();
	virtual void SendStreamEnd();
	virtual void Reset();

private:
	typedef std::set<Listener*> Listeners;
protected:
	DWORD		id = 0;
	DWORD		data = 0;
	std::wstring	tag;
	Listeners	listeners;
	Use		lock;
	DWORD		rtt = 0;
};

class RTMPPipedMediaStream :
	public RTMPMediaStream,
	public RTMPMediaStream::Listener
{
public:
	RTMPPipedMediaStream();
	RTMPPipedMediaStream(DWORD id);
	virtual ~RTMPPipedMediaStream();

	void SetWaitIntra(bool waitIntra)			{ this->waitIntra = waitIntra; }
	void SetRewriteTimestamps(bool rewriteTimestamps)	{ this->rewriteTimestamps = rewriteTimestamps; }
	void Attach(RTMPMediaStream *stream);
	void Detach();

	//Override method
	virtual DWORD AddMediaListener(RTMPMediaStream::Listener* listener);

	/** RTMPMediaStrem listener events used for attaching to other RTMPMediaStreams*/
	virtual void onAttached(RTMPMediaStream *stream);
	virtual void onMediaFrame(DWORD id,RTMPMediaFrame *frame);
	virtual void onMetaData(DWORD id,RTMPMetaData *meta);
	virtual void onCommand(DWORD id,const wchar_t *name,AMFData* obj);
	virtual void onStreamBegin(DWORD id);
	virtual void onStreamEnd(DWORD id);
	//virtual void onStreamIsRecorded(DWORD id);
	virtual void onStreamReset(DWORD id);
	virtual void onDetached(RTMPMediaStream *stream);
protected:
	RTMPMediaStream*	attached;
	RTMPMetaData*		meta;
	RTMPVideoFrame*		desc;
	RTMPAudioFrame*		aacSpecificConfig;
	QWORD	first;
	bool	waitIntra;
	bool	rewriteTimestamps;
};

class RTMPCachedPipedMediaStream : public RTMPPipedMediaStream
{
public:
	virtual ~RTMPCachedPipedMediaStream();
	void Clear();
	//Override method
	virtual DWORD AddMediaListener(RTMPMediaStream::Listener* listener);
	virtual void SendMediaFrame(RTMPMediaFrame *frame);
public:
	typedef std::list<RTMPMediaFrame*> FrameChache;
public:
	FrameChache cached;
	Use use;
};

class RTMPNetStream : 
	public RTMPPipedMediaStream
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onNetStreamStatus(DWORD streamId, QWORD transId, const RTMPNetStatusEventInfo &info,const wchar_t *message) = 0;
		virtual void onNetStreamDestroyed(DWORD id) = 0;
	};
	
	using shared = std::shared_ptr<RTMPNetStream>;
public:
	RTMPNetStream(DWORD id,Listener *listener);
	virtual ~RTMPNetStream();

	//Netstream API
	virtual void doPlay(QWORD transId, std::wstring& url,RTMPMediaStream::Listener *listener);
	virtual void doPublish(QWORD transId, std::wstring& url);
	virtual void doPause(QWORD transId);
	virtual void doResume(QWORD transId);
	virtual void doSeek(QWORD transId, DWORD time);
	virtual void doCommand(RTMPCommandMessage *cmd);
	virtual void doClose(QWORD transId, RTMPMediaStream::Listener *listener);
	//virtual void doPlay2();
	//virtual void doReceiveAudio(bool flag);
	//virtual void doReceiveVideo(bool flag);
	//virtual void doReceiveVideoFPS(bool flag;)
	//virtual void doSend(std::wstring &name,args);
	//virtual void doStep(DWORD num);
	//virtual void doTogglePause();
	virtual void ProcessCommandMessage(RTMPCommandMessage* cmd);
	
protected:
	virtual void fireOnNetStreamStatus(QWORD transId, const RTMPNetStatusEventInfo &info,const wchar_t* message);

protected:
	Listener*		listener;
};

class RTMPMediaStreamPlayer
{
public:
	virtual bool Play(std::wstring& url,RTMPMediaStream *stream) = 0;
	virtual bool Seek(DWORD time) = 0;
	virtual bool Pause() = 0;
	virtual bool Resume() = 0;
	virtual bool Close();
};

class RTMPMediaStreamPublisher : public RTMPMediaStream::Listener
{
public:
	virtual bool Publish(std::wstring& url) = 0;
	virtual void PublishMediaFrame(RTMPMediaFrame *frame) = 0;
	virtual bool Close() = 0;
};

#endif
