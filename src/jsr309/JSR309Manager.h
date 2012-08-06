/* 
 * File:   JSR309Manager.h
 * Author: Sergio
 *
 * Created on 8 de septiembre de 2011, 13:06
 */

#ifndef JSR309MANAGER_H
#define	JSR309MANAGER_H

#include <map>
#include "config.h"
#include "MediaSession.h"
#include "xmlstreaminghandler.h"

class JSR309Manager : public MediaSession::Listener
{
public:
	enum Events
	{
		PlayerEndOfFileEvent = 1
	};
public:
	JSR309Manager();
	virtual ~JSR309Manager();

	int Init(XmlStreamingHandler *eventMngr);
	int End();

	int CreateEventQueue();
	int DeleteEventQueue(int id);
	int CreateMediaSession(std::wstring tag,int queueId);
	int GetMediaSessionRef(int id,MediaSession **sess);
	int ReleaseMediaSessionRef(int id);
	int DeleteMediaSession(int id);

	//Events
	virtual void onPlayerEndOfFile(MediaSession *sess,Player *player,int playerId,void *param);

private:
	struct MediaSessionEntry : public Use
	{
		int id;
		int enabled;
		int queueId;
		std::wstring tag;
		MediaSession* sess;
	};

	typedef std::map<int,MediaSessionEntry*> MediaSessions;

private:
	XmlStreamingHandler *eventMngr;
	MediaSessions	sessions;
	pthread_mutex_t	mutex;
	int maxId;
	bool inited;
};

class PlayerEndOfFileEvent: public XmlEvent
{
public:
	PlayerEndOfFileEvent(std::wstring &sessTag,std::wstring &playerTag)
	{
		//Get session tag
		UTF8Parser sessTagParser(sessTag);
		UTF8Parser playerTagParser(playerTag);

		//Serialize
		DWORD sessLen = sessTagParser.Serialize(this->sessTag,1024);
		DWORD playerLen  = playerTagParser.Serialize(this->playerTag,1024);

		//Set end
		this->sessTag[sessLen] = 0;
		this->playerTag[playerLen] = 0;
	}
	
	virtual xmlrpc_value* GetXmlValue(xmlrpc_env *env)
	{
		return xmlrpc_build_value(env,"(iss)",(int)JSR309Manager::PlayerEndOfFileEvent,sessTag,playerTag);
	}
private:
	BYTE sessTag[1024];
	BYTE playerTag[1024];
};


#endif	/* JSR309MANAGER_H */

