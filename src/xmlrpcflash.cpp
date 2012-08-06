#include "xmlhandler.h"
#include "flash.h"

//StarPlaying
xmlrpc_value* StartPlaying(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Flash *flash = (Flash *)user_data;

	//Parseamos
	char *ip;
	int audioPort;
	int videoPort;
	char *url;
	xmlrpc_parse_value(env, param_array, "(siis)", &ip,&audioPort,&videoPort,&url);
	 
	//Start playing
	int id = flash->StartPlaying(ip,audioPort,videoPort,url);

	//Chewck for errors
	if (!id>0)
		return xmlerror(env,"Couldn't start playback");


	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",id));
}

xmlrpc_value* StopPlaying(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	Flash *flash = (Flash *)user_data;

	 //Parseamos
	int id;
	xmlrpc_parse_value(env, param_array, "(i)", &id);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

	//Stop playing
	if (!flash->StopPlaying(id))
		return xmlerror(env,"Couldn't stop flash playback\n");

	//Devolvemos el resultado
	return xmlok(env);
}

XmlHandlerCmd flashCmdList[] =
{
	{"StartPlaying",StartPlaying},
	{"StopPlaying",StopPlaying},
	{NULL,NULL}
};
