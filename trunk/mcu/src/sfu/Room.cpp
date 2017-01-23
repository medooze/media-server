#include "log.h"
#include "Room.h"
#include "SFUParticipant.h"

using namespace SFU;

Room::Room(std::wstring tag)
{
	//Store it
	this->tag = tag;
	
	maxId = 1;
}

Room::~Room()
{
	End();
}

int Room::Init()
{
	Log("-Init media room\n");
	//Init bundle transport
	return bundle.Init();
}

int Room::End()
{
	Log("-End media room\n");
	//End media transport
	return bundle.End();
}

int Room::AddParticipant(std::wstring name,Properties &properties)
{
	Log(">Room::AddParticipant [%ls]\n",name.c_str());
	
	//For each property
	for (Properties::const_iterator it=properties.begin();it!=properties.end();++it)
		Debug("-Room::AddParticipant | Setting property [%s:%s]\n",it->first.c_str(),it->second.c_str());
		
	std::string username;
	
	//Get ice username
	username += properties.GetProperty("ice.localUsername");
	username += ":";
	username += properties.GetProperty("ice.remoteUsername");
	
	//Create transport
	DTLSICETransport* transport = bundle.AddICETransport(username,properties);
	
	//If there was an error on transport parameters
	if (!transport)
		//Error
		return Error("-Room::AddParticipant error creating dtls ice transport\n");
	
	//Create participant
	Participant *participant = new Participant(transport);
	
	//Add local stream
	participant->AddRemoteStream(properties);
	//Get remote self view
	participant->AddListener(participant);
	participant->AddLocalStream(participant->GetStream());
	//For all other participants
	for (auto it=participants.begin();it!=participants.end();++it)
	{
		//Get the other participant
		Participant *other = it->second;
		//Add each other as listener
		other->AddListener(participant);
		participant->AddListener(other);
		//Add local streams also
		other->AddLocalStream(participant->GetStream());
		participant->AddLocalStream(other->GetStream());
	}
	//Get new id
	DWORD partId = maxId++;
	
	//Add it
	participants[partId] = participant;
	
	Log("<Room::AddParticipant [partId:%d]\n",partId);
	
	//Return id
	return partId;
}

int Room::RemoveParticipant(int partId)
{
	Log("-Room::RemoveParticipant [%d]\n",partId);
	
	//El iterator
	Participants::iterator it = participants.find(partId);

	//Si no esta
	if (it == participants.end())
	{
		//Unlock
		//participantsLock.Unlock();
		//Exit
		return Error("Participant not found\n");
	}

	//LO obtenemos
	Participant *participant = it->second;

	//Y lo quitamos del mapa
	participants.erase(it);
	
	//For all other participants
	for (auto it=participants.begin();it!=participants.end();++it)
	{
		//Get the other participant
		Participant *other = it->second;
		//Add each other as listener
		other->RemoveListener(participant);
	}
	
	//Delete it
	delete(participant);
	
	//Done
	return 1;
}