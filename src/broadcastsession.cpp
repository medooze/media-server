#include <string>
#include <map>
#include <vector>
#include <set>

#include "broadcastsession.h"

BroadcastSession::BroadcastSession(const std::wstring &tag)
{
        //Save tag
 	this->tag = tag;
	//Not inited
	inited = true;
	sent = 0;
	maxTransfer = 0;
	maxConcurrent = 0;
}

BroadcastSession::~BroadcastSession()
{
	//End just in case
	End();
}

bool BroadcastSession::Init(DWORD maxTransfer,DWORD maxConcurrent)
{
	//Set default limits
	this->maxTransfer = maxTransfer;
	this->maxConcurrent = maxConcurrent;

	//Reset sent bytes
	sent = 0;
	//We are started
	inited = true;

	return true;
}

bool BroadcastSession::End()
{
	//Check if we are running
	if (!inited)
		//Exit
		return false;

	//Lock
	lock.WaitUnusedAndLock();

	//Stop
	inited=0;

	//For each connection watcher
	for (Watchers::iterator it=watchers.begin(); it!=watchers.end(); ++it )
		//Delete nc
		delete(*it);
	//clear
	watchers.clear();
	//For each connection watcher
	for (Publishers::iterator it=publishers.begin(); it!=publishers.end(); ++it )
		//Delete nc
		delete(*it);
	//clear
	publishers.clear();
	//For each media stream
	for (MediaSreams::iterator it=streams.begin(); it!=streams.end(); ++it )
		//Delete stream
		delete(it->second);
	//clear streams
	streams.clear();

	//Unlock
	lock.Unlock();
	
	//For each
	return true;
}

bool   BroadcastSession::GetBroadcastPublishedStreams(BroadcastSession::PublishedStreamsInfo &list)
{
	//Shared lock
	lock.IncUse();
	//For each media stream
	for (MediaSreams::iterator it=streams.begin(); it!=streams.end(); ++it )
	{
		//Get info
		StreamInfo  info;
		//Set vlues
		info.name = it->first;
		info.url = it->second->GetTag();
		info.ip = "unknown yet";
		//Insert
		list[info.name] = info;
	}
		
	//Unlock
	lock.DecUse();
	//Ok
	return true;
}

RTMPNetConnection* BroadcastSession::ConnectPublisher(RTMPNetConnection::Listener* listener)
{
	NetConnection* publisher = NULL;

	//Shared lock
	lock.IncUse();
	
	//Check if inited
	if (!inited)
		//End
		goto unlock;
	//Create new connection publisher
	publisher = new NetConnection(NetConnection::Publisher,this);
	//Connect
	publisher->AddListener(listener);
	//Add to watchers
	publishers.insert(publisher);

unlock:
	//Unlock
	lock.DecUse();

	//Return publisher
	return publisher;
}
RTMPNetConnection* BroadcastSession::ConnectWatcher(RTMPNetConnection::Listener* listener)
{
	NetConnection* watcher = NULL;

	//Shared lock
	lock.IncUse();

	//Check if inited
	if (!inited)
		//End
		goto unlock;
	//Create new connection publisher
	watcher = new NetConnection(NetConnection::Watcher,this);
	//Connect
	watcher->AddListener(listener);
	//Add to watchers
	watchers.insert(watcher);

unlock:
	//Unlock
	lock.DecUse();

	//Return publisher
	return watcher;
}
void BroadcastSession::onDisconnected(NetConnection* connection)
{
	//Check
	if (!connection)
		//Exit
		return;

	//Shared lock
	lock.IncUse();

	//Check type
	if (connection->GetType()==NetConnection::Publisher)
		//Remove from publishers
		publishers.erase(connection);
	else
		//Remove from watchers
		watchers.erase(connection);
	//Delete it
	delete(connection);
	//Unlock
	lock.DecUse();
}

RTMPPipedMediaStream* BroadcastSession::PlayMediaStream(const std::wstring &name,WatcherNetStream *netStream)
{
	RTMPPipedMediaStream *stream = NULL;
	//Lock
	lock.IncUse();
	//Find it
	MediaSreams::iterator it = streams.find(name);
	//Check if found
	if (it!=streams.end())
	{
		//Get stream
		stream = it->second;
		//Attach player to stream
		netStream->Attach(stream);
	}
	//Unlock
	lock.DecUse();

	//return
	return stream;
}

RTMPPipedMediaStream* BroadcastSession::PublishMediaStream(const std::wstring &url,PublisherNetStream *netStream)
{
	RTMPPipedMediaStream *stream = NULL;
	
	//Parse url
	std::wstring name;;

	//Find query string
	int pos = url.find(L"?");

	//Remove query string
	if (pos!=std::wstring::npos)
		//Remove it
		name = url.substr(0,pos);
	else
		//Set all
		name = url;

	Log(">Publish stream [%ls]\n",name.c_str());

	//Lock
	lock.IncUse();
	//Find it
	MediaSreams::iterator it = streams.find(name);
	//Check if found
	if (it!=streams.end())
	{
		Log(">RePublishing stream\n");
		//Get stream
		stream = it->second;
		//Attach to publisher stream
		it->second->Attach(netStream);
		Log("<RePublished stream\n");
	} else{
		//Log
		Log("-Publishing new stream [name:%ls,url:%ls]\n",name.c_str(),url.c_str());
		//Create just one media stream with tag name
		stream = new RTMPCachedPipedMediaStream();
		//Set wait for intra
		stream->SetWaitIntra(true);
		//Do not override
		stream->SetRewriteTimestamps(false);
		//Add it
		streams[name] = stream;
		//Store url
		stream->SetTag(url);
		//Attach to publisher stream
		stream->Attach(netStream);
		//Log
		Log("-Retransmitting stream [name:%ls]\n",name.c_str());
		//For each retransmitter
		for (Transmitters::iterator it=transmitters.begin(); it!=transmitters.end(); ++it)
		{
			//Release stream
			(*it)->Call(L"releaseStream",new AMFNull,new AMFString(name));
			//Publish
			(*it)->Call(L"FCPublish",new AMFNull,new AMFString(name));
			//Create stream
			(*it)->CreateStream(name);
		}
	}
	//Unlock
	lock.DecUse();

	Log("<Published stream [%ls]\n",name.c_str());
	
	//return
	return stream;
}
BroadcastSession::NetConnection::NetConnection(Type type,BroadcastSession *sess)
{
	//Store
	this->type = type;
	this->sess = sess;
}

BroadcastSession::NetConnection::Type BroadcastSession::NetConnection::GetType()
{
	//Return type
	return type;
}

 RTMPNetStream::shared BroadcastSession::NetConnection::CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener* listener)
{
	RTMPNetStream::shared stream;

	//Check type
	if (type==Publisher)
		//Create publisher stream
		stream = std::make_shared<PublisherNetStream>(streamId,sess,listener);
	else
		//Create watcher stream
		stream = std::make_shared<WatcherNetStream>(streamId,sess,listener);
	//Register it
	RegisterStream(stream);
	//return it
	return stream;
}

void BroadcastSession::NetConnection::DeleteStream(const RTMPNetStream::shared& stream)
{
	//Unregister
	UnRegisterStream(stream);
}

void BroadcastSession::NetConnection::Disconnect(RTMPNetConnection::Listener* listener)
{
	//Discnnect and delete me
	sess->onDisconnected(this);
}

BroadcastSession::PublisherNetStream::PublisherNetStream(DWORD id,BroadcastSession *sess,Listener *listener) : RTMPNetStream(id,listener)
{
	//Store session
	this->sess = sess;
	//No stream
	stream = NULL;
	//Do not wait for first intra
	SetWaitIntra(false);
	//Do not override ts
	SetRewriteTimestamps(false);
}

BroadcastSession::PublisherNetStream::~PublisherNetStream()
{
	//Close
	Close();
}

void BroadcastSession::PublisherNetStream::doPublish(QWORD transId, std::wstring& url)
{
	//Publish
	sess->PublishMediaStream(url,this);
	//Publish ok
	fireOnNetStreamStatus(transId,RTMP::Netstream::Publish::Start,L"Publish started");
}
void BroadcastSession::PublisherNetStream::doClose(QWORD transId, RTMPMediaStream::Listener *listener)
{
	//Close
	Close();
}

void BroadcastSession::PublisherNetStream::Close()
{
	Log(">Close broadcaster netstream\n");

	//End stream
	SendStreamEnd();

	//Check stream
	if (stream)
		//Remove us from listener
		stream->RemoveMediaListener(this);
	///Remove listener just in case
	RemoveAllMediaListeners();
	//Dettach if playing
	Detach();

	Log("<Closed\n");
}

void BroadcastSession::PublisherNetStream::onDetached(RTMPMediaStream* stream)
{
	Log("-onDetached broadcaster netstream\n");
	//No stream
	stream = NULL;
}

BroadcastSession::WatcherNetStream::WatcherNetStream(DWORD id,BroadcastSession *sess,Listener *listener) : RTMPNetStream(id,listener)
{
	//Store session
	this->sess = sess;
	//No stream
	stream = NULL;
	//Wait for first intra
	SetWaitIntra(true);
	//Override TS
	SetRewriteTimestamps(true);
}
BroadcastSession::WatcherNetStream::~WatcherNetStream()
{
	//Close
	Close();
}
void BroadcastSession::WatcherNetStream::doPlay(QWORD transId, std::wstring& url,RTMPMediaStream::Listener *listener)
{
	//Play
	if (stream=sess->PlayMediaStream(url,this))
	{
		//Add listener
		AddMediaListener(listener);
		//Send reseted status
		fireOnNetStreamStatus(transId,RTMP::Netstream::Play::Reset,L"Playback reset");
		//Send play status
		fireOnNetStreamStatus(transId,RTMP::Netstream::Play::Start,L"Playback started");
	} else {
		//Send error
		fireOnNetStreamStatus(transId,RTMP::Netstream::Play::StreamNotFound,L"Stream not found");
	}
}

void BroadcastSession::WatcherNetStream::doClose(QWORD transId, RTMPMediaStream::Listener *listener)
{
	//Close
	Close();
}

void BroadcastSession::WatcherNetStream::Close()
{
	Log(">Close broadcaster netstream\n");

	//Check stream
	if (stream)
		//Remove us from listener
		stream->RemoveMediaListener(this);
	///Remove listener just in case
	RemoveAllMediaListeners();
	//Dettach if playing
	Detach();

	Log("<Closed\n");
}

void BroadcastSession::WatcherNetStream::onDetached(RTMPMediaStream* stream)
{
	Log("-onDetached broadcaster netstream\n");
	//No stream
	stream = NULL;
}

void BroadcastSession::onConnected(RTMPClientConnection* conn)
{
	//For each media stream
	for (MediaSreams::iterator it=streams.begin(); it!=streams.end(); ++it )
	{
		//Release stream
		conn->Call(L"releaseStream",new AMFNull,new AMFString(it->first));
		//Publish
		conn->Call(L"FCPublish",new AMFNull,new AMFString(it->first));
		//Create stream
		conn->CreateStream(it->first);
	}
}

void BroadcastSession::onNetStreamCreated(RTMPClientConnection* conn,RTMPClientConnection::NetStream *stream)
{
	//Get associated stream name
	std::wstring name = stream->GetTag();

	//Log
	Log("-Transmitter onNetStreamCreated [tag:%ls]\n",name.c_str());
	
	//Find it
	MediaSreams::iterator it=streams.find(name);
	//If not found
	if (it==streams.end())
	{
		//Delete it
		conn->DeleteStream(stream);
		//Exit
		return;
	}
	//Get published url
	std::wstring url = it->second->GetTag();
	//Do publish url
	stream->Publish(url);
	//Attach to the stream
	stream->Attach(it->second);
}

void BroadcastSession::onCommandResponse(RTMPClientConnection* conn,DWORD id,bool isError,AMFData* param)
{

}

void BroadcastSession::onDisconnected(RTMPClientConnection* conn)
{

}
