#include "log.h"
#include "xmlhandler.h"
#include "broadcaster.h"

xmlrpc_value* BroadcasterCreateBroadcast(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser nameParser;
        UTF8Parser tagParser;
	Broadcaster *broadcaster = (Broadcaster *)user_data;
	BroadcastSession *session = NULL;

	 //Parseamos
	char *str;
        char *tag;
	int maxTransfer;
	int maxConcurrent;
	xmlrpc_parse_value(env, param_array, "(ssii)", &str,&tag,&maxTransfer,&maxConcurrent);

	//Parse string
	nameParser.Parse((BYTE*)str,strlen(str));
        tagParser.Parse((BYTE*)tag,strlen(tag));
	 
	//Creamos la sessionerencia
	int sessionId = broadcaster->CreateBroadcast(nameParser.GetWString(),tagParser.GetWString());
	
	//Obtenemos la referencia
	if(!broadcaster->GetBroadcastRef(sessionId,&session))
		return xmlerror(env,"Conferencia borrada antes de poder iniciarse\n");

	//Si error
	if (!sessionId>0)
		return xmlerror(env,"Could not create session");

	//La iniciamos
	bool res = session->Init(maxTransfer,maxConcurrent);

	//Liberamos la referencia
	broadcaster->ReleaseBroadcastRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not start conference\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",sessionId));
}

xmlrpc_value* BroadcasterDeleteBroadcast(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Broadcaster *broadcaster = (Broadcaster *)user_data;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Delete sessionerence 
	if (!broadcaster->DeleteBroadcast(sessionId))
		return xmlerror(env,"Session does not exist");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* BroadcasterPublishBroadcast(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Broadcaster *broadcaster = (Broadcaster *)user_data;

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
	bool res = broadcaster->PublishBroadcast(sessionId,parser.GetWString());

	//Salimos
	if(!res)
		return xmlerror(env,"Could not add pin to broadcast session\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* BroadcasterUnPublishBroadcast(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Broadcaster *broadcaster = (Broadcaster *)user_data;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Delete sessionerence 
	if (!broadcaster->UnPublishBroadcast(sessionId))
		return xmlerror(env,"Session does not exist");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* BroadcasterAddBroadcastToken(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	Broadcaster *broadcaster = (Broadcaster *)user_data;

	 //Parseamos
	int sessionId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &sessionId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));
	 
	//Add token
	bool res = broadcaster->AddBroadcastToken(sessionId,parser.GetWString());

	//Salimos
	if(!res)
		return xmlerror(env,"Could not add pin to broadcast session\n");

	//Devolvemos el resultado
	return xmlok(env);
}
xmlrpc_value* GetBroadcastPublishedStreams(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	
	BroadcastSession::PublishedStreamsInfo list;

	Broadcaster *broadcaster = (Broadcaster *)user_data;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Obtenemos la referencia
	if(!broadcaster->GetBroadcastPublishedStreams(sessionId,list))
		//Error
		return xmlerror(env,"Could not retreive publshed streams info list\n");

	//Create array
	xmlrpc_value* arr = xmlrpc_array_new(env);

	//Process result
	for (BroadcastSession::PublishedStreamsInfo::iterator it = list.begin(); it!=list.end(); ++it)
	{
		char name[1024];
		char url[1024];
		UTF8Parser utf8name(it->second.name);
		UTF8Parser utf8url(it->second.url);
		//Serialize
		int len = utf8name.Serialize((BYTE*)name,1024);
		name[len] = 0;
		len = utf8url.Serialize((BYTE*)url,1024);
		url[len] = 0;
		//Create array
		xmlrpc_value* val = xmlrpc_build_value(env,"(sss)",name,url,it->second.ip.c_str());
		//Add it
		xmlrpc_array_append_item(env,arr,val);
		//Release
		xmlrpc_DECREF(val);
	}

	//return
	return xmlok(env,arr);
}

XmlHandlerCmd broadcasterCmdList[] =
{
	{"CreateBroadcast",BroadcasterCreateBroadcast},
	{"DeleteBroadcast",BroadcasterDeleteBroadcast},
	{"PublishBroadcast",BroadcasterPublishBroadcast},
	{"UnPublishBroadcast",BroadcasterUnPublishBroadcast},
	{"AddBroadcastToken",BroadcasterAddBroadcastToken},
	{"GetBroadcastPublishedStreams",GetBroadcastPublishedStreams},
	{NULL,NULL}
};
