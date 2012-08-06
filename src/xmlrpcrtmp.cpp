#include "xmlhandler.h"
#include "FLVServer.h"

//CreateSession
xmlrpc_value* FLVCreateSession(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	FLVServer *flv = (FLVServer *)user_data;
	FLVSession *session = NULL;

	 //Parseamos
	char *str;
	xmlrpc_parse_value(env, param_array, "(s)", &str);
	 
	//Creamos la sessionerencia
	int sessionId = flv->CreateSession(str);

	//Si error
	if (!sessionId>0)
		return xmlerror(env,"No se puede crear la sessionerencia");

	//Obtenemos la referencia
	if(!flv->GetSessionRef(sessionId,&session))
		return xmlerror(env,"Conferencia borrada antes de poder iniciarse\n");

	//La iniciamos
	int port = session->Init();

	//Liberamos la referencia
	flv->ReleaseSessionRef(sessionId);

	//Salimos
	if(!port)
		return xmlerror(env,"No se ha podido iniciar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(ii)",sessionId,port));
}

xmlrpc_value* FLVDeleteSession(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	FLVServer *flv = (FLVServer *)user_data;
	FLVSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Delete sessionerence 
	if (!flv->DeleteSession(sessionId))
		return xmlerror(env,"La sessionerencia no existe\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* FLVStartReceivingVideo(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	FLVServer *flv = (FLVServer *)user_data;
	FLVSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;
	 
	//Obtenemos la referencia
	if(!flv->GetSessionRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int recVideoPort = session->StartReceivingVideo();

	//Liberamos la referencia
	flv->ReleaseSessionRef(sessionId);

	//Salimos
	if(!recVideoPort)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recVideoPort));
}

xmlrpc_value* FLVStopReceivingVideo(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	FLVServer *flv = (FLVServer *)user_data;
	FLVSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;
	 
	//Obtenemos la referencia
	if(!flv->GetSessionRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopReceivingVideo();

	//Liberamos la referencia
	flv->ReleaseSessionRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* FLVStartReceivingAudio(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	FLVServer *flv = (FLVServer *)user_data;
	FLVSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;
	 
	//Obtenemos la referencia
	if(!flv->GetSessionRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int recAudioPort = session->StartReceivingAudio();

	//Liberamos la referencia
	flv->ReleaseSessionRef(sessionId);

	//Salimos
	if(!recAudioPort)
		return xmlerror(env,"No se ha podido terminar la sessionerencia\n");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recAudioPort,recAudioPort));
}

xmlrpc_value* FLVStopReceivingAudio(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	FLVServer *flv = (FLVServer *)user_data;
	FLVSession *session = NULL;

	 //Parseamos
	int sessionId;
	xmlrpc_parse_value(env, param_array, "(i)", &sessionId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;
	 
	//Obtenemos la referencia
	if(!flv->GetSessionRef(sessionId,&session))
		return xmlerror(env,"La sessionerencia no existe\n");

	//La borramos
	int res = session->StopReceivingAudio();

	//Liberamos la referencia
	flv->ReleaseSessionRef(sessionId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error\n");

	//Devolvemos el resultado
	return xmlok(env);
}

XmlHandlerCmd flvCmdList[] =
{
	{"CreateSession",FLVCreateSession},
	{"DeleteSession",FLVDeleteSession},
	{"StartReceivingVideo",FLVStartReceivingVideo},
	{"StopReceivingVideo",FLVStopReceivingVideo},
	{"StartReceivingAudio",FLVStartReceivingAudio},
	{"StopReceivingAudio",FLVStopReceivingAudio},
	{NULL,NULL}
};
