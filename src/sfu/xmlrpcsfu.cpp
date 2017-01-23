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
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId,partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Delete roomerence
	if (!room->RemoveParticipant(partId))
		return xmlerror(env,"Participant does not exist");

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
/*
xmlrpc_value* PlayerCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &roomId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int playerId = room->PlayerCreate(parser.GetWString());

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(playerId<0)
		return xmlerror(env,"Could not set input token for media bridge\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",playerId));
}

xmlrpc_value* PlayerDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int playerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->PlayerDelete(playerId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* PlayerOpen(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int playerId;
	char *filename;
	xmlrpc_parse_value(env, param_array, "(iis)", &roomId, &playerId,&filename);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->PlayerOpen(playerId,filename);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not open player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* PlayerPlay(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int playerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->PlayerPlay(playerId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not play player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* PlayerSeek(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int playerId;
	int time;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId, &playerId,&time);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->PlayerSeek(playerId,time);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not seek player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* PlayerStop(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int playerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId,&playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->PlayerStop(playerId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not stop player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* PlayerClose(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int playerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId,&playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->PlayerClose(playerId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not stop player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RecorderCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &roomId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create recorder
	int recorderId = room->RecorderCreate(parser.GetWString());

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(recorderId<0)
		return xmlerror(env,"Could not set input token for media bridge\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recorderId));
}

xmlrpc_value* RecorderDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int recorderId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &recorderId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->RecorderDelete(recorderId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete recorder\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RecorderRecord(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int recorderId;
	char *filename;
	xmlrpc_parse_value(env, param_array, "(iis)", &roomId, &recorderId, &filename);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->RecorderRecord(recorderId,filename);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not play recorder\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RecorderStop(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int recorderId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId,&recorderId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->RecorderStop(recorderId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not stop recorder\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RecorderAttachToEndpoint(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int recorderId;
	int endpointId;
	int media;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&recorderId,&endpointId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->RecorderAttachToEndpoint(recorderId,endpointId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RecorderAttachToAudioMixerPort(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int recorderId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&recorderId,&mixerId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->RecorderAttachToAudioMixerPort(recorderId,mixerId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RecorderAttachToVideoMixerPort(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int recorderId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&recorderId,&mixerId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->RecorderAttachToVideoMixerPort(recorderId,mixerId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RecorderDettach(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int recorderId;
	int media;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&recorderId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->RecorderDettach(recorderId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	char *str;
	int audioSupported;
	int videoSupported;
	int textSupported;
	xmlrpc_parse_value(env, param_array, "(isbbb)", &roomId,&str,&audioSupported, &videoSupported, &textSupported);


	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int endpointId = room->EndpointCreate(parser.GetWString(),audioSupported,videoSupported,textSupported);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(endpointId<0)
		return xmlerror(env,"Could not set input token for media bridge\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",endpointId));
}

xmlrpc_value* EndpointDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int endpointId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &endpointId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->EndpointDelete(endpointId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete endpoint\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointSetLocalCryptoSDES(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int endpointId;
	int media;
	char *suite;
	char *key;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &roomId,&endpointId,&media,&suite,&key);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointSetLocalCryptoSDES(endpointId,(MediaFrame::Type)media,suite,key);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointSetRemoteCryptoSDES(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int endpointId;
	int media;
	char *suite;
	char *key;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &roomId,&endpointId,&media,&suite,&key);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointSetRemoteCryptoSDES(endpointId,(MediaFrame::Type)media,suite,key);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);
	
	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointSetRemoteCryptoDTLS(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int endpointId;
	int media;
	char *setup;
	char *hash;
	char *fingerprint;
	xmlrpc_parse_value(env, param_array, "(iiisss)", &roomId,&endpointId,&media,&setup,&hash,&fingerprint);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointSetRemoteCryptoDTLS(endpointId,(MediaFrame::Type)media,setup,hash,fingerprint);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointGetLocalCryptoDTLSFingerprint(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	std::string fingerprint;
	char *hash;
	xmlrpc_parse_value(env, param_array, "(s)", &hash);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Check hash
	if (strcasecmp(hash,"sha-1")==0)
		//Gen fingerprint
		fingerprint = DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA1);
	else if (strcasecmp(hash,"sha-256")==0)
		
	else
		return 0;
	//Create array
	xmlrpc_value* arr = xmlrpc_build_value(env,"(s)",fingerprint.c_str());

	//return
	return xmlok(env,arr);
}
xmlrpc_value* EndpointSetLocalSTUNCredentials(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int endpointId;
	int media;
	char *username;
	char *pwd;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &roomId,&endpointId,&media,&username,&pwd);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointSetLocalSTUNCredentials(endpointId,(MediaFrame::Type)media,username,pwd);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointSetRemoteSTUNCredentials(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int endpointId;
	int media;
	char *username;
	char *pwd;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &roomId,&endpointId,&media,&username,&pwd);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointSetRemoteSTUNCredentials(endpointId,(MediaFrame::Type)media,username,pwd);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}


xmlrpc_value* EndpointSetRTPProperties(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int media;
	xmlrpc_value *map;
	xmlrpc_parse_value(env, param_array, "(iiiS)", &roomId,&endpointId,&media,&map);

	//Get the rtp map
	Properties properties;

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

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointSetRTPProperties(endpointId,(MediaFrame::Type)media,properties);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);

}

xmlrpc_value* EndpointStartSending(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int media;
	int res = 0;
	char *sendIp;
	int sendPort;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiisiS)", &roomId,&endpointId,&media,&sendIp,&sendPort,&rtpMap);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Get the rtp map
	RTPMap map;

	//Get map size
	int j = xmlrpc_struct_size(env,rtpMap);

	//Parse rtp map
	for (int i=0;i<j;i++)
	{
		xmlrpc_value *key, *val;
		const char *type;
		int codec;
		//Read member
		xmlrpc_struct_read_member(env,rtpMap,i,&key,&val);
		//Read name
		xmlrpc_parse_value(env,key,"s",&type);
		//Read value
		xmlrpc_parse_value(env,val,"i",&codec);
		//Add to map
		map[atoi(type)] = (VideoCodec::Type) codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Start sending video
	res = room->EndpointStartSending(endpointId,(MediaFrame::Type)media,sendIp,sendPort,map);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointStopSending(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int media;
	int res = 0;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&endpointId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Stop sending video
	res = room->EndpointStopSending(endpointId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"No se ha podido terminar la roomerencia\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointStartReceiving(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int media;
	int recPort = 0;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiS)", &roomId,&endpointId,&media,&rtpMap);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");
	
	//Get the rtp map
	RTPMap map;

	//Get map size
	int j = xmlrpc_struct_size(env,rtpMap);

	//Parse rtp map
	for (int i=0;i<j;i++)
	{
		xmlrpc_value *key, *val;
		const char *type;
		int codec;
		//Read member
		xmlrpc_struct_read_member(env,rtpMap,i,&key,&val);
		//Read name
		xmlrpc_parse_value(env,key,"s",&type);
		//Read value
		xmlrpc_parse_value(env,val,"i",&codec);
		//Add to map
		map[atoi(type)] = (VideoCodec::Type) codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Start receiving video and get listening port
	recPort = room->EndpointStartReceiving(endpointId,(MediaFrame::Type)media,map);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!recPort)
		return xmlerror(env,"No se ha podido terminar la roomerencia\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recPort));
}

xmlrpc_value* EndpointStopReceiving(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int media;
	int res = 0;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&endpointId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Stop sending video
	res = room->EndpointStopReceiving(endpointId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointRequestUpdate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int media;
	int res = 0;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&endpointId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Stop sending text
	res = room->EndpointRequestUpdate(endpointId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointAttachToPlayer(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int playerId;
	int media;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&endpointId,&playerId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointAttachToPlayer(endpointId,playerId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointAttachToEndpoint(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int sourceId;
	int media;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&endpointId,&sourceId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointAttachToEndpoint(endpointId,sourceId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointAttachToAudioMixerPort(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&endpointId,&mixerId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointAttachToAudioMixerPort(endpointId,mixerId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointAttachToVideoMixerPort(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&endpointId,&mixerId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointAttachToVideoMixerPort(endpointId,mixerId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointAttachToVideoTranscoder(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int videoTranscoderId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&endpointId,&videoTranscoderId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointAttachToVideoTranscoder(endpointId,videoTranscoderId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* EndpointDettach(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int endpointId;
	int media;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&endpointId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->EndpointDettach(endpointId,(MediaFrame::Type)media);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* AudioMixerCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &roomId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int mixerId = room->AudioMixerCreate(parser.GetWString());

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(mixerId<0)
		return xmlerror(env,"Could not create media mixer\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mixerId));
}

xmlrpc_value* AudioMixerDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &mixerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->AudioMixerDelete(mixerId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* AudioMixerPortCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(iis)", &roomId, &mixerId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int portId = room->AudioMixerPortCreate(mixerId,parser.GetWString());

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(portId<0)
		return xmlerror(env,"Could not set input token for media bridge\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",portId));
}


xmlrpc_value* AudioMixerPortSetCodec(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	int portId;
	int codec;
	Properties properties;
	xmlrpc_value *map;

	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId, &mixerId, &portId, &codec);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->AudioMixerPortSetCodec(mixerId,portId,(AudioCodec::Type)codec);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete player\n");

	//Devolvemos el resultado
	return xmlok(env);
}


xmlrpc_value* AudioMixerPortDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId, &mixerId, &portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->AudioMixerPortDelete(mixerId,portId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete mixer port\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* AudioMixerPortAttachToPlayer(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int portId;
	int playerId;

	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&mixerId,&portId,&playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->AudioMixerPortAttachToPlayer(mixerId,portId,playerId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* AudioMixerPortAttachToEndpoint(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int portId;
	int endpointId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&mixerId,&portId,&endpointId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->AudioMixerPortAttachToEndpoint(mixerId,portId,endpointId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* AudioMixerPortDettach(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&mixerId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->AudioMixerPortDettach(mixerId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &roomId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int mixerId = room->VideoMixerCreate(parser.GetWString());

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(mixerId<0)
		return xmlerror(env,"Could not create media mixer\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mixerId));
}

xmlrpc_value* VideoMixerDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &mixerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->VideoMixerDelete(mixerId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete player\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerPortCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	char *str;
	int mosaicId;
	xmlrpc_parse_value(env, param_array, "(iisi)", &roomId, &mixerId, &str, &mosaicId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int portId = room->VideoMixerPortCreate(mixerId,parser.GetWString(),mosaicId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(portId<0)
		return xmlerror(env,"Could not set input token for media bridge\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",portId));
}

xmlrpc_value* VideoMixerPortDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId, &mixerId, &portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->VideoMixerPortDelete(mixerId,portId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete mixer port\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerPortSetCodec(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	int portId;
	int codec;
	int size;
	int fps;
	int bitrate;
	int intraPeriod;
	Properties properties;
	xmlrpc_value *map;
	xmlrpc_parse_value(env, param_array, "(iiiiiiiiS)", &roomId, &mixerId, &portId, &codec, &size, &fps, &bitrate, &intraPeriod);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

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

        //Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;
        
	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int res = room->VideoMixerPortSetCodec(mixerId,portId,(VideoCodec::Type)codec,size,fps,bitrate,intraPeriod,properties);
	
	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerPortAttachToPlayer(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int portId;
	int playerId;

	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&mixerId,&portId,&playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerPortAttachToPlayer(mixerId,portId,playerId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerPortAttachToEndpoint(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int portId;
	int endpointId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&mixerId,&portId,&endpointId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerPortAttachToEndpoint(mixerId,portId,endpointId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerPortDettach(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&mixerId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerPortDettach(mixerId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerMosaicCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	int comp;
	int size;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId, &mixerId, &comp, &size);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int mosaicId = room->VideoMixerMosaicCreate(mixerId,(Mosaic::Type)comp,size);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(mosaicId<0)
		return xmlerror(env,"Could not set input token for media bridge\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mosaicId));
}

xmlrpc_value* VideoMixerMosaicDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int mixerId;
	int mosaicId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId, &mixerId, &mosaicId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->VideoMixerMosaicDelete(mixerId,mosaicId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete mixer port\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerMosaicSetSlot(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int portId;
	int mosaicId;
	int num;

	xmlrpc_parse_value(env, param_array, "(iiiii)", &roomId,&mixerId,&mosaicId,&num,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerMosaicSetSlot(mixerId,mosaicId,num,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerMosaicSetCompositionType(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int mosaicId;
	int comp;
	int size;
	xmlrpc_parse_value(env, param_array, "(iiiii)", &roomId,&mixerId,&mosaicId,&comp,&size);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerMosaicSetCompositionType(mixerId,mosaicId,(Mosaic::Type)comp,size);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerMosaicSetOverlayPNG(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int mosaicId;
	char* overlay;
	xmlrpc_parse_value(env, param_array, "(iiis)", &roomId,&mixerId,&mosaicId,&overlay);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerMosaicSetOverlayPNG(mixerId,mosaicId,overlay);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerMosaicResetOverlay(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int mosaicId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&mixerId,&mosaicId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerMosaicResetSetOverlay(mixerId,mosaicId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerMosaicAddPort(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int mosaicId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&mixerId,&mosaicId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerMosaicAddPort(mixerId,mosaicId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoMixerMosaicRemovePort(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int mixerId;
	int mosaicId;
	int portId;
	xmlrpc_parse_value(env, param_array, "(iiii)", &roomId,&mixerId,&mosaicId,&portId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoMixerMosaicRemovePort(mixerId,mosaicId,portId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoTranscoderCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &roomId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int mixerId = room->VideoTranscoderCreate(parser.GetWString());

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(mixerId<0)
		return xmlerror(env,"Could not create video transcoder\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mixerId));
}

xmlrpc_value* VideoTranscoderDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int videoTranscoderId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &videoTranscoderId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"Media room not found\n");

	//La borramos
	bool res = room->VideoTranscoderDelete(videoTranscoderId);

	//Release ref
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete video transcoder\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoTranscoderFPU(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int videoTranscoderId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId, &videoTranscoderId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int res = room->VideoTranscoderFPU(videoTranscoderId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoTranscoderSetCodec(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Room *room;
	SFUManager *sfu = (SFUManager*)user_data;

	//Parseamos
	int roomId;
	int videoTranscoderId;
	int codec;
	int size;
	int fps;
	int bitrate;
	int intraPeriod;
	Properties properties;
	xmlrpc_value *map;

	xmlrpc_parse_value(env, param_array, "(iiiiiiiS)", &roomId, &videoTranscoderId, &codec, &size, &fps, &bitrate, &intraPeriod, &map);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

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

        //Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//Create player
	int res = room->VideoTranscoderSetCodec(videoTranscoderId,(VideoCodec::Type)codec,size,fps,bitrate,intraPeriod, properties);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoTranscoderAttachToEndpoint(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int videoTranscoderId;
	int endpointId;
	xmlrpc_parse_value(env, param_array, "(iii)", &roomId,&videoTranscoderId,&endpointId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoTranscoderAttachToEndpoint(videoTranscoderId,endpointId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* VideoTranscoderDettach(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	SFUManager *sfu = (SFUManager*)user_data;
	Room *room = NULL;

	 //Parseamos
	int roomId;
	int videoTranscoderId;
	xmlrpc_parse_value(env, param_array, "(ii)", &roomId,&videoTranscoderId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!sfu->GetRoomRef(roomId,&room))
		return xmlerror(env,"The media Session does not exist");

	//La borramos
	int res = room->VideoTranscoderDettach(videoTranscoderId);

	//Liberamos la referencia
	sfu->ReleaseRoomRef(roomId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}
*/

XmlHandlerCmd sfuCmdList[] =
{
	{"EventQueueCreate",			EventQueueCreate},
	{"EventQueueDelete",			EventQueueDelete},
	{"RoomCreate",				RoomCreate},
	{"RoomAddParticipant",			RoomAddParticipant},
	{"RoomDelete",				RoomDelete},
	/*
	{"PlayerCreate",			PlayerCreate},
	{"PlayerOpen",				PlayerOpen},
	{"PlayerPlay",				PlayerPlay},
	{"PlayerSeek",				PlayerSeek},
	{"PlayerStop",				PlayerStop},
	{"PlayerClose",				PlayerClose},
	{"PlayerDelete",			PlayerDelete},
	{"RecorderCreate",			RecorderCreate},
	{"RecorderRecord",			RecorderRecord},
	{"RecorderStop",			RecorderStop},
	{"RecorderDelete",			RecorderDelete},
	{"RecorderAttachToEndpoint",		RecorderAttachToEndpoint},
	{"RecorderAttachToAudioMixerPort",	RecorderAttachToAudioMixerPort},
	{"RecorderAttachToVideoMixerPort",	RecorderAttachToVideoMixerPort},
	{"RecorderDettach",			RecorderDettach},
	{"EndpointCreate",			EndpointCreate},
	{"EndpointDelete",			EndpointDelete},
	{"EndpointSetRemoteCryptoSDES",		EndpointSetRemoteCryptoSDES},
	{"EndpointSetLocalCryptoSDES",		EndpointSetLocalCryptoSDES},
	{"EndpointSetLocalSTUNCredentials",	EndpointSetLocalSTUNCredentials},
	{"EndpointSetRemoteSTUNCredentials",	EndpointSetRemoteSTUNCredentials},
	{"EndpointSetRemoteCryptoDTLS",		EndpointSetRemoteCryptoDTLS},
	{"EndpointSetRTPProperties",		EndpointSetRTPProperties},
	{"EndpointStartSending",		EndpointStartSending},
	{"EndpointStopSending",			EndpointStopSending},
	{"EndpointStartReceiving",		EndpointStartReceiving},
	{"EndpointStopReceiving",		EndpointStopReceiving},
	{"EndpointRequestUpdate",		EndpointRequestUpdate},
	{"EndpointAttachToPlayer",		EndpointAttachToPlayer},
	{"EndpointAttachToEndpoint",		EndpointAttachToEndpoint},
	{"EndpointAttachToAudioMixerPort",	EndpointAttachToAudioMixerPort},
	{"EndpointAttachToVideoMixerPort",	EndpointAttachToVideoMixerPort},
	{"EndpointAttachToVideoTranscoder",	EndpointAttachToVideoTranscoder},
	{"EndpointDettach",			EndpointDettach},
	{"AudioMixerCreate",			AudioMixerCreate},
	{"AudioMixerDelete",			AudioMixerDelete},
	{"AudioMixerPortCreate",		AudioMixerPortCreate},
	{"AudioMixerPortSetCodec",		AudioMixerPortSetCodec},
	{"AudioMixerPortDelete",		AudioMixerPortDelete},
	{"AudioMixerPortAttachToEndpoint",	AudioMixerPortAttachToEndpoint},
	{"AudioMixerPortAttachToPlayer",	AudioMixerPortAttachToPlayer},
	{"AudioMixerPortDettach",		AudioMixerPortDettach},
	{"VideoMixerCreate",			VideoMixerCreate},
	{"VideoMixerDelete",			VideoMixerDelete},
	{"VideoMixerPortCreate",		VideoMixerPortCreate},
	{"VideoMixerPortDelete",		VideoMixerPortDelete},
	{"VideoMixerPortSetCodec",		VideoMixerPortSetCodec},
	{"VideoMixerPortAttachToEndpoint",	VideoMixerPortAttachToEndpoint},
	{"VideoMixerPortAttachToPlayer",	VideoMixerPortAttachToPlayer},
	{"VideoMixerPortDettach",		VideoMixerPortDettach},
	{"VideoMixerMosaicCreate",		VideoMixerMosaicCreate},
	{"VideoMixerMosaicDelete",		VideoMixerMosaicDelete},
	{"VideoMixerMosaicSetSlot",		VideoMixerMosaicSetSlot},
	{"VideoMixerMosaicSetCompositionType",	VideoMixerMosaicSetCompositionType},
	{"VideoMixerMosaicSetOverlayPNG",	VideoMixerMosaicSetOverlayPNG},
	{"VideoMixerMosaicResetOverlay",	VideoMixerMosaicResetOverlay},
	{"VideoMixerMosaicAddPort",		VideoMixerMosaicAddPort},
	{"VideoMixerMosaicRemovePort",		VideoMixerMosaicRemovePort},
	{"VideoTranscoderCreate",		VideoTranscoderCreate},
	{"VideoTranscoderDelete",		VideoTranscoderDelete},
	{"VideoTranscoderSetCodec",		VideoTranscoderSetCodec},
	{"VideoTranscoderFPU",			VideoTranscoderFPU},
	{"VideoTranscoderAttachToEndpoint",	VideoTranscoderAttachToEndpoint},
	{"VideoTranscoderDettach",		VideoTranscoderDettach}
	*/
	{NULL,NULL}
};

