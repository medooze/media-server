#ifndef SFUMANAGER_H
#define	SFUMANAGER_H

#include <map>
#include "config.h"
#include "Room.h"
#include "xmlstreaminghandler.h"

class SFUManager
{
public:
	SFUManager();
	virtual ~SFUManager();

	int Init(XmlStreamingHandler *eventMngr);
	int End();

	int CreateEventQueue();
	int DeleteEventQueue(int id);
	int CreateRoom(std::wstring tag,int queueId);
	int GetRoomRef(int id,SFU::Room **room);
	int ReleaseRoomRef(int id);
	int DeleteRoom(int id);

private:
	struct RoomEntry : public Use
	{
		int id;
		int enabled;
		int queueId;
		std::wstring tag;
		SFU::Room* room;
	};

	typedef std::map<int,RoomEntry*> Rooms;

private:
	XmlStreamingHandler *eventMngr;
	Rooms	rooms;
	pthread_mutex_t	mutex;
	int maxId;
	bool inited;
};

#endif	/* SFUMANAGER_H */

