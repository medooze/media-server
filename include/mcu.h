#ifndef _MCU_H_
#define _MCU_H_
#include <string>
#include <set>
#include <memory>
#include "pthread.h"
#include "multiconf.h"
#include "rtmp/rtmpstream.h"
#include "rtmp/rtmpapplication.h"
#include "xmlstreaminghandler.h"
#include "uploadhandler.h"
#include "ws/websocketserver.h"
#include "CPUMonitor.h"


class MCU : 
	public RTMPApplication,
	public MultiConf::Listener,
	public WebSocketServer::Handler,
	public UploadHandler::Listener,
	public CPUMonitor::Listener
{
public:
	enum Events
	{
		ParticipantRequestFPU = 1,
		CPULoadInfo = 2
	};

	struct ConferenceInfo
	{
		int id;
		std::wstring name;
		int numPart;
	};

	typedef  std::map<int,ConferenceInfo> ConferencesInfo;
public:
	MCU();
	~MCU();

	int Init(XmlStreamingHandler *eventMngr);
	int CreateEventQueue();
	int DeleteEventQueue(int id);
	int End();

	int CreateConference(std::wstring tag,int queueId);
	MultiConf::shared GetConferenceRef(int id);
	int GetConferenceId(const std::wstring& tag);
	int ReleaseConferenceRef(int id);
	int DeleteConference(int confId);
	int GetConferenceList(ConferencesInfo& lst);


	/** CPU montior*/
	virtual void onCPULoad(int user, int sys, int load, int numcpu);
	/** Conference events*/
	virtual void onParticipantRequestFPU(MultiConf *conf,int partId,void *param);
	/** RTMP application interface*/
	virtual RTMPNetConnection::shared Connect(const std::wstring& appName,RTMPNetConnection::Listener *listener,std::function<void(bool)> accept);
	/** File uploader event */
	virtual int onFileUploaded(const char* url, const char *filename);
	/** WebSocket connection event */
	virtual void onWebSocketConnection(const HTTPRequest& request,WebSocket *ws);

private:
	class ConferenceEntry : public Use
	{
	public:
		int id;
		int enabled;
		int queueId;
		std::shared_ptr<MultiConf> conf;
	};

	typedef std::map<int,ConferenceEntry*> Conferences;
	typedef std::map<std::wstring,int> ConferenceTags;
	typedef std::set<int>	EventQueues;
private:
	XmlStreamingHandler	*eventMngr;
	Conferences		conferences;
	ConferenceTags		tags;
	int			maxId;
	Use			use;
	int inited;
	EventQueues		eventQueues;
};

class CPULoadInfoEvent : public XmlEvent
{
public:
	CPULoadInfoEvent(int user,int sys,int load,int cpus)
	{
		this->user = user;
		this->sys  = sys;
		this->load = load;
		this->cpus = cpus;
	}
	virtual xmlrpc_value* GetXmlValue(xmlrpc_env *env)
	{
		return xmlrpc_build_value(env,"(iiiii)",(int)MCU::CPULoadInfo,user,sys,load,cpus);
	}
private:
	int user;
	int sys;
	int load;
	int cpus;
};

class PlayerRequestFPUEvent: public XmlEvent
{
public:
	PlayerRequestFPUEvent(int confId,std::wstring &tag,int partId)
	{
		//Get session tag
		UTF8Parser tagParser(tag);
		
		//Serialize
		DWORD len = tagParser.Serialize(this->tag,1024);

		//Set end
		this->tag[len] = 0;
		//Store other values
		this->confId = confId;
		this->partId = partId;
	}

	virtual xmlrpc_value* GetXmlValue(xmlrpc_env *env)
	{
		return xmlrpc_build_value(env,"(iisi)",(int)MCU::ParticipantRequestFPU,confId,tag,partId);
	}
private:
	int confId;
	BYTE tag[1024];
	int partId;
};

#endif
