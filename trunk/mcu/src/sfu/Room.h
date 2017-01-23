/* 
 * File:   Room.h
 * Author: Sergio
 *
 * Created on 6 de septiembre de 2011, 15:55
 */

#ifndef ROOM_H
#define	ROOM_H

#include "config.h"
#include "media.h"
#include "codecs.h"
#include "video.h"
#include "RTPBundleTransport.h"
#include "SFUParticipant.h"

namespace SFU
{
class Room 
{

public:
	Room(std::wstring tag);
	~Room();

	int Init();
	int AddParticipant(std::wstring name,Properties &properties);
	int RemoveParticipant(int partId);
	int End();

	//Getters
	std::wstring& GetTag()		{ return tag;				}
	DWORD GetTransportPort() const	{ return bundle.GetLocalPort();	}
private:
	typedef std::map<int,Participant*> Participants;
private:
	std::wstring tag;
	int maxId;
	Participants participants;
	RTPBundleTransport bundle;
};
}
#endif	/* MEDIASESSION_H */

