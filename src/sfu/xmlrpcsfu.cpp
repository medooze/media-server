#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmlhandler.h"
#include "SFUManager.h"
#include "Room.h"
#include "codecs.h"
#include "amf.h"

using namespace SFU;

inline xmlrpc_value* EventQueueCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
        SFUManager *sfu = (SFUManager*)user_data;

	//Create the event queue
	int queueId = sfu->CreateEventQueue();

	//Si error
	if (!queueId>0)
		return xmlerror(env,"Could not create event queue");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",queueId));
}

inline xmlrpc_value* EventQueueDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;

	 //Parseamos
	int queueId;
	xmlrpc_parse_value(env, param_array, "(i)", &queueId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Delete event queue
	if (!sfu->DeleteEventQueue(queueId))
		return xmlerror(env,"Event queue does not exist\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RoomCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	const char* hash = "sha-256";
	std::string fingerprint;
	
	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Create array
	xmlrpc_value* arr = xmlrpc_build_value(env,"(s)",fingerprint.c_str());
	UTF8Parser nameParser;
        SFUManager *sfu = (SFUManager*)user_data;

	 //Parseamos
	char *str;
	int queueId;
        xmlrpc_parse_value(env, param_array, "(si)", &str,&queueId);

	//Parse string
	nameParser.Parse((BYTE*)str,strlen(str));

	//Creamos la roomerencia
	int roomId = sfu->CreateRoom(nameParser.GetWString(),queueId);
	
	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Get port for transport
	int port = room->GetTransportPort();

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Gen fingerprint
	fingerprint = DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA256);

	//Si error
	if (!roomId>0)
		return xmlerror(env,"Could not create room");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(iiss)",roomId,port,hash,fingerprint.c_str()));
}

xmlrpc_value* RoomAddParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;
	UTF8Parser parser;

	 //Parseamos
	int roomId;
	char *sname;
	Properties properties;
	xmlrpc_value *map;
	xmlrpc_parse_value(env, param_array, "(isS)", &roomId,&sname,&map);
	
	//Get map size
	int j = xmlrpc_struct_size(env,map);

	//Parse rtp map
	for (int i=0;i<j;i++)
	{
		xmlrpc_value *key, *val;
		const char *strKey;
		const char *strVal;
		//Read member
		xmlrpc_struct_read_member(env,map,i,&key,&val);
		//Read name
		xmlrpc_parse_value(env,key,"s",&strKey);
		//Read value
		xmlrpc_parse_value(env,val,"s",&strVal);
		//Add to map
		properties[strKey] = strVal;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Parse string
	parser.Parse((BYTE*)sname,strlen(sname));

	//Get name
	std::wstring name = parser.GetWString();

	//Add participant
	int partId = room->AddParticipant(name,properties);

	//Free conference reference
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!partId)
		return xmlerror(env,"Error creating participant");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",partId));
}

xmlrpc_value* RoomRemoveParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;
	 //Parseamos
	int roomId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId,&partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Delete roomerence
	if (!room->RemoveParticipant(partId))
		return xmlerror(env,"Participant does not exist");
	
	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Devolvemos el resultado
	return xmlok(env);
}


xmlrpc_value* ParticipantSelectLayer(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;
	 //Parseamos
	int roomId;
	int partId;
	int spatialLayerId;
	int temporalLayerId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&partId,&spatialLayerId,&temporalLayerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Delete roomerence
	room->ParticipantSelectLayer(partId,spatialLayerId,temporalLayerId);
	
	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Devolvemos el resultado
	return xmlok(env);
}


xmlrpc_value* RoomDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;

	 //Parseamos
	int roomId;
	xmlrpc_parse_value(env, param_array, "(i)", &roomId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Delete roomerence
	if (!sfu->DeleteRoom(roomId))
		return xmlerror(env,"Session does not exist");

	//Devolvemos el resultado
	return xmlok(env);
}



XmlHandlerCmd sfuCmdList[] =
{
	{"EventQueueCreate",			EventQueueCreate},
	{"EventQueueDelete",			EventQueueDelete},
	{"RoomCreate",				RoomCreate},
	{"RoomAddParticipant",			RoomAddParticipant},
	{"RoomDelete",				RoomDelete},
	{"ParticipantSelectLayer",		ParticipantSelectLayer},
	{NULL,NULL}
};

