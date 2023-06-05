#include <sys/poll.h>

#include "log.h"
#include "rtmp/rtmpstream.h"

RTMPMediaStream::RTMPMediaStream()
{
	this->id = 0;
	this->data = 0;
}
RTMPMediaStream::RTMPMediaStream(DWORD id)
{
	this->id = id;
	this->data = 0;
}

RTMPMediaStream::~RTMPMediaStream()
{
	//Remove all listeners
	RemoveAllMediaListeners();
}

DWORD RTMPMediaStream::AddMediaListener(Listener *listener)
{
	Log("-RTMPMediaStream::AddMediaListener() [id:%d,listener:%p]\n",id,listener); 
	
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Apend
	listeners.insert(listener);
	//Get number of listeners
	DWORD num = listeners.size();
	//Attached
	listener->onAttached(this);
	//Unlock
	lock.Unlock();
	//return number of listeners
	return num;
}

void RTMPMediaStream::RemoveAllMediaListeners()
{
	Log("-RTMPMediaStream::RemoveAllMediaListeners() [id:%d]\n",id);
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//For each listener
	for(Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Detach it
		(*it)->onDetached(this);
	//Clean listeners
	listeners.clear();
	//Unlock
	lock.Unlock();
}

DWORD RTMPMediaStream::RemoveMediaListener(Listener *listener)
{
	Log("-RTMPMediaStream::RemoveMediaListener() [id:%d,listener:%p]\n",id,listener);
	
	//Lock mutexk
	lock.WaitUnusedAndLock();
	//Find it
	Listeners::iterator it = listeners.find(listener);
	//If present
	if (it!=listeners.end())
	{
		//Detach
		listener->onDetached(this);
		//erase it
		listeners.erase(it);
	}
	//Get number of listeners
	DWORD num = listeners.size();
	//Unlock
	lock.Unlock();
	//return number of listeners
	return num;
}

void RTMPMediaStream::SendMediaFrame(RTMPMediaFrame* frame)
{
	//Lock mutexk
	lock.IncUse();
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Send it
		(*it)->onMediaFrame(id,frame);
	//Unlock
	lock.DecUse();
}

void RTMPMediaStream::SendCommand(const wchar_t *name,AMFData* obj)
{
	//Lock mutexk
	lock.IncUse();
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Send it
		(*it)->onCommand(id,name,obj);
	//Unlock
	lock.DecUse();
}

void RTMPMediaStream::SendMetaData(RTMPMetaData* meta)
{
	//Lock mutexk
	lock.IncUse();
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Send it
		(*it)->onMetaData(id,meta);
	//Unlock
	lock.DecUse();
}

void RTMPMediaStream::SendStreamEnd()
{
	//Lock mutexk
	lock.IncUse();
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Send it
		(*it)->onStreamEnd(id);
	//Unlock
	lock.DecUse();
}

void RTMPMediaStream::SendStreamBegin()
{
	//Lock mutexk
	lock.IncUse();
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Send it
		(*it)->onStreamBegin(id);
	//Unlock
	lock.DecUse();
}

void RTMPMediaStream::Reset()
{
	//Lock mutexk
	lock.IncUse();
	//Iterate
	for (Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
		//Send it
		(*it)->onStreamReset(id);
	//Unlock
	lock.DecUse();
}
/*****************************
 * RTMPPipedMediaStream
 ****************************/
RTMPPipedMediaStream::RTMPPipedMediaStream() : RTMPMediaStream(0)
{
	//Not attached
	attached = NULL;
	//No first frame
	first = (QWORD)-1;
	//NO meta
	meta = NULL;
	//No desc
	desc = NULL;
	aacSpecificConfig = NULL;
	//Do not wait for intra
	waitIntra = false;
	//Rewrite ts on default
	rewriteTimestamps = true;
}
RTMPPipedMediaStream::RTMPPipedMediaStream(DWORD id) : RTMPMediaStream(id)
{
	//Not attached
	attached = NULL;
	//No first frame
	first = (QWORD)-1;
	//NO meta
	meta = NULL;
	//No desc
	desc = NULL;
	aacSpecificConfig = NULL;
	//Do not wait for intra
	waitIntra = false;
	//Rewrite ts on default
	rewriteTimestamps = true;
}

RTMPPipedMediaStream::~RTMPPipedMediaStream()
{
	//Dettach just in case
	Detach();
	//Delete meta
	if (meta)
		//Delete it
		delete(meta);
	//Delete desc
	if (desc)
		//Delete it
		delete(desc);
	//Delete aac config
	if (aacSpecificConfig)
		//Delete it
		delete(aacSpecificConfig);
}

DWORD RTMPPipedMediaStream::AddMediaListener(RTMPMediaStream::Listener *listener)
{
	//Check meta
	if (meta)
		//Add media listener
		listener->onMetaData(id,meta);
	//Check desc
	if (desc)
		//Send it
		listener->onMediaFrame(id,desc);
	//return number of listeners
	return RTMPMediaStream::AddMediaListener(listener);
}

void RTMPPipedMediaStream::Attach(RTMPMediaStream *stream)
{
	//If attached to the same stream
	if (stream==attached)
		//Do nothing
		return;
        //Detach if joined
	if (attached)
		//Remove ourself as listeners
		attached->RemoveMediaListener(this);
	//If it is not null
	if (stream)
		//Join to the new one
		stream->AddMediaListener(this);
	//Set it anyway
	attached = stream;
}

void RTMPPipedMediaStream::Detach()
{
        //Detach if joined
	if (attached)
		//Remove ourself as listeners
		attached->RemoveMediaListener(this);
	//Detach it anyway
	attached = NULL;
}

void RTMPPipedMediaStream::onAttached(RTMPMediaStream *stream)
{
	//Check if attached to another stream
	if (attached)
		//Remove ourself as listeners
		attached->RemoveMediaListener(this);
	//Store new one
	attached = stream;
}
void RTMPPipedMediaStream::onDetached(RTMPMediaStream *stream)
{
	//Detach if joined
	if (attached && attached!=stream)
		//Remove ourself as listeners
		attached->RemoveMediaListener(this);
	//Detach
	attached = NULL;
}

void RTMPPipedMediaStream:: onMediaFrame(DWORD id,RTMPMediaFrame *frame)
{
	//Get timestamp
	QWORD ts = frame->GetTimestamp();

	//Check if it is not set
	if (ts==(QWORD)-1)
		//Reuse it
		SendMediaFrame(frame);
	
	//Check if it is first
	if (first==(QWORD)-1)
	{
		//If we have to wait to video
		if (waitIntra)
		{
			//Get AAC config
			if (frame->GetType()==RTMPMediaFrame::Audio)
			{
				//Check if it is aac config frame
				RTMPAudioFrame* audio = (RTMPAudioFrame*)frame;

				//check frame type
				if (audio->GetAudioCodec()==RTMPAudioFrame::AAC && audio->GetAACPacketType()==RTMPAudioFrame::AACSequenceHeader)
					//clone aac config
					aacSpecificConfig = (RTMPAudioFrame*)audio->Clone();
				//Skip frame
				return;
			}

			//Check if it is intra video frame
			RTMPVideoFrame* video = (RTMPVideoFrame*)frame;

			//check frame type
			if (video->GetFrameType()!=RTMPVideoFrame::INTRA && video->GetFrameType()!=RTMPVideoFrame::GENERATED_KEY_FRAME)
				//Skip
				return;
			
			//Check if it is h264
			if (video->GetVideoCodec()==RTMPVideoFrame::AVC)
			{
				//Check type
				if(video->GetAVCType()==RTMPVideoFrame::AVCHEADER)
				{
					//Check if we already had one desc
					if (desc)
						//Delete it
						delete(desc);
					//Clone it
					desc = (RTMPVideoFrame*)video->Clone();
					//Set 0 timestamp
					desc->SetTimestamp(0);
					//Keep it for later
					return;
				} 
				//Check if we have desc
				if (!desc)
					//Skip
					return;
			}
		}
		//We got it
		Log("-Got first frame [%llu,waitIntra:%d,type:%s]\n",ts,waitIntra,RTMPMediaFrame::GetTypeName(frame->GetType()));
		//Send stream begin
		SendStreamBegin();
		//Store timestamp
		first = ts;
		//Check if we have meta
		if (meta)
			//Send it
			SendMetaData(meta);
		//Check
		if (desc)
			//Send previous desc before frame
			SendMediaFrame(desc);
		//Check aac config
		if (aacSpecificConfig)
			//Send it
			SendMediaFrame(aacSpecificConfig);
	}

	//Check if we have to rewrite ts
	if (rewriteTimestamps)
	{
		//Make sure it is not an out of order packet
		if (ts<first)
		{
			//Exit
			 Error("ERROR: RTMP media frame ts before first one, dropping it!! [ts:%llu,first:%llu]\n",ts,first);
			 return;
		}
		//Modify timestamp
		frame->SetTimestamp(ts-first);
	}

	//Send it
	SendMediaFrame(frame);

	//Check if we have rewriten ts
	if (rewriteTimestamps)
		//Set previous
		frame->SetTimestamp(ts);
}

void RTMPPipedMediaStream:: onMetaData(DWORD id,RTMPMetaData *publishedMetaData)
{
	//Check method
	AMFString* name = (AMFString*)publishedMetaData->GetParams(0);

	//Get timestamp
	QWORD ts = publishedMetaData->GetTimestamp();

	//Check it
	if (name->GetWString().compare(L"@setDataFrame")==0)
	{
		//If we already had one
		if (meta)
			//Delete if already have one
			delete(meta);

		//Create new msg
		meta = new RTMPMetaData(0);

		//Copy the rest of params
		for (DWORD i=1;i<publishedMetaData->GetParamsLength();i++)
			//Clone and append
			meta->AddParam(publishedMetaData->GetParams(i)->Clone());

		//Check if we have started to send it
		if (first!=(QWORD)-1)
		{
			//Check if we have to rewrite ts
			if (rewriteTimestamps)
				//Set new meta
				meta->SetTimestamp(ts-first);
			//Send it back
			SendMetaData(meta);
		} 
	} else {
		
		RTMPMetaData *cloned = publishedMetaData->Clone();
		//Check if we have to rewrite ts
		if (rewriteTimestamps)
		{

			//Check if we have started to send it
			if (first!=(QWORD)-1)
				//Set new meta
				cloned->SetTimestamp(ts-first);
			else
				//Now
				cloned->SetTimestamp(0);
		}
		//Send it back
		SendMetaData(cloned);
		
	}
}

void RTMPPipedMediaStream:: onCommand(DWORD id,const wchar_t *name,AMFData* obj)
{
	//Do nothing with commands
}

void RTMPPipedMediaStream:: onStreamBegin(DWORD id)
{
}

void RTMPPipedMediaStream:: onStreamEnd(DWORD id)
{
}

void RTMPPipedMediaStream:: onStreamReset(DWORD id)
{
	//Reset
	Reset();
}
 RTMPCachedPipedMediaStream::~RTMPCachedPipedMediaStream()
 {
	 //Free memory
	 Clear();
 }

void RTMPCachedPipedMediaStream::Clear()
{
	//Lock
	use.WaitUnusedAndLock();
	//Get frame
	for (FrameChache::iterator it = cached.begin(); it!=cached.end(); ++it)
		//Delete frame
		delete(*it);
	//Cleare list
	cached.clear();
	//Unlock
	use.Unlock();
}

DWORD RTMPCachedPipedMediaStream::AddMediaListener(RTMPMediaStream::Listener* listener)
{
	
	//return number of listeners
	int num = RTMPMediaStream::AddMediaListener(listener);
	
	//Send meta if available
	if (meta)
		//Add media listener
		listener->onMetaData(id,meta);
	//Check desc
	if (desc)
		//Send it
		listener->onMediaFrame(id,desc);
	//Lock cache
	use.IncUse();
	//Send all queued frames up to now
	//Get frame
	for (FrameChache::iterator it = cached.begin(); it!=cached.end(); ++it)
		//Send it
		listener->onMediaFrame(id,*it);
	//Dec use
	use.DecUse();
	//Return it
	return num;
}
void RTMPCachedPipedMediaStream::SendMediaFrame(RTMPMediaFrame *frame)
{
	//Check if it is video intra
	if (frame->GetType()==RTMPMediaFrame::Video && ((RTMPVideoFrame*)frame)->GetFrameType()==RTMPVideoFrame::INTRA)
		//Clear cache
		Clear();
	//Append to queue
	cached.push_back(frame->Clone());
	//Call parent
	RTMPPipedMediaStream::SendMediaFrame(frame);
}

/****************************
 * RTMPNetStream
 ***************************/
RTMPNetStream::RTMPNetStream(DWORD id,Listener *listener) : RTMPPipedMediaStream(id)
{
	Log("-RTMPNetStream:RTMPNetStream() [id:%d,listener:%p,this:%p]\n",id,listener,this); 
	//Store listener
	this->listener = listener;
}

RTMPNetStream::~RTMPNetStream()
{
	Log("-RTMPNetStream::~RTMPNetStream() [id:%d,listener:%p,this:%p]\n",id,listener,this); 
	if (listener)
		listener->onNetStreamDestroyed(id);
}

void  RTMPNetStream::doPlay(QWORD transId, std::wstring& url,RTMPMediaStream::Listener *listener)
{
	fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Play not supported in this stream");
}
void  RTMPNetStream::doPublish(QWORD transId, std::wstring& url)
{
	fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Publish not supported in this stream");
}
void  RTMPNetStream::doPause(QWORD transId)
{
	fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Pause not supported in this stream");
}
void  RTMPNetStream::doResume(QWORD transId)
{
	fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Resume not supported in this stream");
}
void  RTMPNetStream::doSeek(QWORD transId, DWORD time)
{
	fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Seek not supported in this stream");
}
void  RTMPNetStream::doClose(QWORD transId, RTMPMediaStream::Listener *listener)
{
	fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Close not supported in this stream");
}

void RTMPNetStream::doCommand(RTMPCommandMessage *cmd)
{
	cmd->Dump();
}
void  RTMPNetStream::fireOnNetStreamStatus(QWORD transId, const RTMPNetStatusEventInfo &info,const wchar_t* message)
{
	if (listener)
		listener->onNetStreamStatus(id,transId, info,message);
}

void RTMPNetStream::ProcessCommandMessage(RTMPCommandMessage* cmd)
{
	//Get message values
	std::wstring name 	= cmd->GetName();
	QWORD transId = cmd->GetTransId();

	//Check command names
	if (name.compare(L"play")==0)
	{
		//Get url to play
		std::wstring url = *(cmd->GetExtra(0));
		//Play
		doPlay(transId, url,this);
	//Publish
	} else if (name.compare(L"publish")==0){
		//Get param
		AMFData *obj = cmd->GetExtra(0);
		//Check type
		if (obj->CheckType(AMFData::String))
		{
			//Get url to play
			std::wstring url = *obj;
			//Publish
			doPublish(transId, url);
		} else {
			//Close
			doClose(transId, this);
		}
	} else if (name.compare(L"seek")==0) {
		//Get timestamp
		double time = *(cmd->GetExtra(0));
		//Play
		doSeek(transId, time);
	} else if (name.compare(L"pause")==0) {
		//Get pause/resume flag
		bool flag = *(cmd->GetExtra(0));
		//Check if it is pause or resume
		if (!flag)
			//Pause
			doPause(transId);
		else
			//Resume
			doResume(transId);
	} else if (name.compare(L"closeStream")==0) {
		//Close stream
		doClose(transId, this);
	} else {
		//Send command
		doCommand(cmd);
	}
}
