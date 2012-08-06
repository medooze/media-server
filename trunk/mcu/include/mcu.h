#ifndef _MCU_H_
#define _MCU_H_
#include <string>
#include "pthread.h"
#include "multiconf.h"
#include "rtmpstream.h"
#include "rtmpapplication.h"
#include "xmlstreaminghandler.h"



class MCU : 
	public RTMPApplication,
	public MultiConf::Listener
{
public:
	enum Events
	{
		ParticipantRequestFPU = 1
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
	int GetConferenceRef(int id,MultiConf **conf);
	int ReleaseConferenceRef(int id);
	int DeleteConference(int confId);
	int GetConferenceList(ConferencesInfo& lst);

	/** Conference events*/
	virtual void onParticipantRequestFPU(MultiConf *conf,int partId,void *param);
	/** RTMP application interface*/
	virtual RTMPNetConnection* Connect(const std::wstring& appName,RTMPNetConnection::Listener* listener);
private:
	struct ConferenceEntry
	{
		int id;
		int numRef;
		int enabled;
		int queueId;
		MultiConf* conf;
	};

	typedef std::map<int,ConferenceEntry*> Conferences;
private:
	XmlStreamingHandler	*eventMngr;
	Conferences		conferences;
	int			maxId;
	pthread_mutex_t		mutex;
	int inited;
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
