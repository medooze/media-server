#include <stdio.h>
#include <stdlib.h>
#include "xmlhandler.h"
#include "mediagateway.h"
#include "codecs.h"

xmlrpc_value* MediaGatewayCreateMediaBridge(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser nameParser;
        MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	char *str;
        xmlrpc_parse_value(env, param_array, "(s)", &str);

	//Parse string
	nameParser.Parse((BYTE*)str,strlen(str));

	//Creamos la sessionerencia
	int sessionId = mediaGateway->CreateMediaBridge(nameParser.GetWString());

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"Session deleted before inited\n");

	//Si error
	if (!sessionId>0)
		return xmlerror(env,"Could not create session");

	//La iniciamos
	bool res = session->Init();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not start bridge\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",sessionId));
}

xmlrpc_value* MediaGatewayDeleteMediaBridge(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Delete sessionerence
	if (!mediaGateway->DeleteMediaBridge(sessionId))
		return xmlerror(env,"Session does not exist");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewaySetMediaBridgeInputToken(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	MediaGateway *mediaGateway = (MediaGateway *)user_data;

	//Parseamos
	int sessionId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &sessionId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//La borramos
	bool res = mediaGateway->SetMediaBridgeInputToken(sessionId,parser.GetWString());

	//Salimos
	if(!res)
		return xmlerror(env,"Could not set input token for media bridge\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewaySetMediaBridgeOutputToken(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	MediaGateway *mediaGateway = (MediaGateway *)user_data;

	//Parseamos
	int sessionId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &sessionId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//La borramos
	bool res = mediaGateway->SetMediaBridgeOutputToken(sessionId,parser.GetWString());

	//Salimos
	if(!res)
		return xmlerror(env,"Could not set output token to bridge session\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStartSendingVideo(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	char *sendVideoIp;
	int sendVideoPort;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(isiS)", &sessionId,&sendVideoIp,&sendVideoPort,&rtpMap);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

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
		map[atoi(type)] = codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StartSendingVideo(sendVideoIp,sendVideoPort,map);

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStopSendingVideo(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopSendingVideo();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStartReceivingVideo(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iS)", &sessionId,&rtpMap);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

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
		map[atoi(type)] = codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int recVideoPort = session->StartReceivingVideo(map);

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!recVideoPort)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recVideoPort));
}

xmlrpc_value* MediaGatewayStopReceivingVideo(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopReceivingVideo();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStartSendingAudio(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	char *sendAudioIp;
	int sendAudioPort;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(isiS)", &sessionId,&sendAudioIp,&sendAudioPort,&rtpMap);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Get the rtp map
	RTPMap map;

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
		map[atoi(type)] = codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StartSendingAudio(sendAudioIp,sendAudioPort,map);

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStopSendingAudio(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopSendingAudio();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStartReceivingAudio(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iS)", &sessionId,&rtpMap);

	//Get the rtp map
	RTPMap map;

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
		map[atoi(type)] = codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}
	
	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int recAudioPort = session->StartReceivingAudio(map);

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!recAudioPort)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recAudioPort,recAudioPort));
}

xmlrpc_value* MediaGatewayStopReceivingAudio(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopReceivingAudio();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStartSendingText(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	char *sendTextIp;
	int sendTextPort;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(isiS)", &sessionId,&sendTextIp,&sendTextPort,&rtpMap);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

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
		map[atoi(type)] = codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StartSendingText(sendTextIp,sendTextPort,map);

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStopSendingText(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopSendingText();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewayStartReceivingText(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iS)", &sessionId,&rtpMap);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

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
		map[atoi(type)] = codec;
		//Decrement ref counter
		xmlrpc_DECREF(key);
		xmlrpc_DECREF(val);
	}

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int recTextPort = session->StartReceivingText(map);

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!recTextPort)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recTextPort));
}

xmlrpc_value* MediaGatewayStopReceivingText(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopReceivingText();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* MediaGatewaySendFPU(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MediaGateway *mediaGateway = (MediaGateway *)user_data;
	MediaBridgeSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!mediaGateway->GetMediaBridgeRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->SendFPU();

	//Liberamos la referencia
	mediaGateway->ReleaseMediaBridgeRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

XmlHandlerCmd mediagatewayCmdList[] =
{
	{"CreateMediaBridge",		MediaGatewayCreateMediaBridge},
	{"SetMediaBridgeInputToken",	MediaGatewaySetMediaBridgeInputToken},
	{"SetMediaBridgeOutputToken",	MediaGatewaySetMediaBridgeOutputToken},
	{"StartSendingVideo",		MediaGatewayStartSendingVideo},
	{"StopSendingVideo",		MediaGatewayStopSendingVideo},
	{"StartReceivingVideo",		MediaGatewayStartReceivingVideo},
	{"StopReceivingVideo",		MediaGatewayStopReceivingVideo},
	{"StartSendingAudio",		MediaGatewayStartSendingAudio},
	{"StopSendingAudio",		MediaGatewayStopSendingAudio},
	{"StartReceivingAudio",		MediaGatewayStartReceivingAudio},
	{"StopReceivingAudio",		MediaGatewayStopReceivingAudio},
	{"StartSendingText",		MediaGatewayStartSendingText},
	{"StopSendingText",		MediaGatewayStopSendingText},
	{"StartReceivingText",		MediaGatewayStartReceivingText},
	{"StopReceivingText",		MediaGatewayStopReceivingText},
	{"DeleteMediaBridge",		MediaGatewayDeleteMediaBridge},
	{"SendFPU",			MediaGatewaySendFPU},
	{NULL,NULL}
};

