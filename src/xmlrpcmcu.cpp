#include <string>
#include <vector>

#include "xmlhandler.h"
#include "mcu.h"

//CreateConference
xmlrpc_value* CreateConference(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;
	UTF8Parser tagParser;
	 //Parseamos
	char *str;
	int vad = 0;
	int queueId = 0;
	xmlrpc_parse_value(env, param_array, "(sii)", &str, &vad, &queueId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		//Send errro
		return xmlerror(env,"Fault occurred");

	//Parse string
	tagParser.Parse((BYTE*)str,strlen(str));

	//Creamos la conferencia
	int confId = mcu->CreateConference(tagParser.GetWString(),queueId);

	//Si error
	if (!confId>0)
		return xmlerror(env,"Error creating conference");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference deleted before initing");

	//La iniciamos
	int res = conf->Init(vad);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not create conference");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",confId));
}

xmlrpc_value* DeleteConference(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	xmlrpc_parse_value(env, param_array, "(i)", &confId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
			return xmlerror(env,"Fault occurred");

	//Delete conference 
	if (!mcu->DeleteConference(confId))
		return xmlerror(env,"Conference does not exist");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* GetConferences(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MCU::ConferencesInfo list;

	//Obtenemos la referencia
	if(!mcu->GetConferenceList(list))
		return xmlerror(env,"Could not retreive conference info list");

	//Create array
	xmlrpc_value* arr = xmlrpc_array_new(env);

	//Process result
	for (MCU::ConferencesInfo::iterator it = list.begin(); it!=list.end(); ++it)
	{
		//Create array
		xmlrpc_value* val = xmlrpc_build_value(env,"(isi)",it->second.id,it->second.name.c_str(),it->second.numPart);
		//Add it
		xmlrpc_array_append_item(env,arr,val);
		//Release
		xmlrpc_DECREF(val);
	}

	//return
	return xmlok(env,arr);
}

xmlrpc_value* CreateMosaic(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int comp;
	int size;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&comp,&size);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int mosaicId = conf->CreateMosaic((Mosaic::Type)comp,size);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!mosaicId)
		return xmlerror(env,"Could not create mosaic");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mosaicId));
}

xmlrpc_value* SetMosaicOverlayImage(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int mosaicId;
	char *filename;
	xmlrpc_parse_value(env, param_array, "(iis)", &confId,&mosaicId,&filename);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetMosaicOverlayImage(mosaicId,filename);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could set overlay image");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mosaicId));
}

xmlrpc_value* ResetMosaicOverlay(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int mosaicId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&mosaicId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->ResetMosaicOverlay(mosaicId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could reset overlay image");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mosaicId));
}
xmlrpc_value* DeleteMosaic(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int mosaicId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&mosaicId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->DeleteMosaic(mosaicId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could delete mosaic");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",mosaicId));
}
xmlrpc_value* CreateSidebar(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	xmlrpc_parse_value(env, param_array, "(i)", &confId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int sidebarId = conf->CreateSidebar();

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!sidebarId)
		return xmlerror(env,"Could not create sidebar");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",sidebarId));
}

xmlrpc_value* DeleteSidebar(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int sidebarId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&sidebarId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->DeleteSidebar(sidebarId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could delete sidebar");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",sidebarId));
}

xmlrpc_value* CreateParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;
	UTF8Parser parser;

	 //Parseamos
	int confId;
	int mosaicId;
	int sidebarId;
	char *name;
	int type;
	xmlrpc_parse_value(env, param_array, "(isiii)", &confId,&name,&type,&mosaicId,&sidebarId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Parse string
	parser.Parse((BYTE*)name,strlen(name));

	//La borramos
	int partId = conf->CreateParticipant(mosaicId,sidebarId,parser.GetWString(),(Participant::Type)type);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!partId)
		return xmlerror(env,"Error creating participant");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",partId));
}

xmlrpc_value* DeleteParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->DeleteParticipant(partId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error deleting participant");

	//Devolvemos el resultado
	return xmlok(env);
}
xmlrpc_value* AddConferenceToken(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(is)", &confId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	bool res  = conf->AddBroadcastToken(parser.GetWString());

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not add pin to broadcast session");

	//Devolvemos el resultado
	return xmlok(env);
}
xmlrpc_value* AddParticipantInputToken(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(iis)", &confId, &partId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	bool res  = conf->AddParticipantInputToken(partId,parser.GetWString());

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not add pin to participant input");

	//Devolvemos el resultado
	return xmlok(env);
}
xmlrpc_value* AddParticipantOutputToken(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	UTF8Parser parser;
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	char *str;
	xmlrpc_parse_value(env, param_array, "(iis)", &confId, &partId, &str);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Parse string
	parser.Parse((BYTE*)str,strlen(str));

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	bool res  = conf->AddParticipantOutputToken(partId,parser.GetWString());

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not add pin to participant output");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StartBroadcaster(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	xmlrpc_parse_value(env, param_array, "(i)", &confId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int port = conf->StartBroadcaster();

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!port)
		return xmlerror(env,"Could not start broadcaster");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",port));
}

xmlrpc_value* StopBroadcaster(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	xmlrpc_parse_value(env, param_array, "(i)", &confId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StopBroadcaster();
	
	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error stoping broadcaster");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StartPublishing(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	char *server;
	int port;
	char* app;
	char* stream;
	xmlrpc_parse_value(env, param_array, "(isiss)", &confId,&server,&port,&app,&stream);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Publish it
	int id = conf->StartPublishing(server,port,app,stream);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!id)
		return xmlerror(env,"Could not start publisihing broadcast");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",id));
}

xmlrpc_value* StopPublishing(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int id;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&id);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Stop publishing
	int res = conf->StopPublishing(id);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error stoping publishing broadcaster");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetVideoCodec(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	//Parseamos
	int confId;
	int partId;
	int codec;
	int mode;
	int fps;
	int bitrate;
	int quality;
	int fillLevel;
	int intraPeriod;
	Properties properties;
	xmlrpc_value *map;
	xmlrpc_parse_value(env, param_array, "(iiiiiiiS)", &confId,&partId,&codec,&mode,&fps,&bitrate,&intraPeriod,&map);

	//Check if it is new api
	if(!env->fault_occurred)
	{
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
	} else {
		//Clean error
		xmlrpc_env_init(env);
		//Parse old values,
		xmlrpc_parse_value(env, param_array, "(iiiiiiiii)", &confId,&partId,&codec,&mode,&fps,&bitrate,&quality,&fillLevel,&intraPeriod);
	}

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetVideoCodec(partId,codec,mode,fps,bitrate,intraPeriod,properties);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"No soportad");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetAudioCodec(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int codec;
	Properties properties;
	xmlrpc_value *map;
	xmlrpc_parse_value(env, param_array, "(iiiS)", &confId,&partId,&codec,&map);

	//Check if it is new api
	if(!env->fault_occurred)
	{
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
	} else {
		//Clean error
		xmlrpc_env_init(env);
		//Parse old values,
		xmlrpc_parse_value(env, param_array, "(iii)", &confId,&partId,&codec);
	}

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetAudioCodec(partId,codec,properties);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"No soportado");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetTextCodec(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int codec;

	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&partId,&codec);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetTextCodec(partId,codec);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"No soportado");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetCompositionType(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int mosaicId;
	int comp;
	int size;
	xmlrpc_parse_value(env, param_array, "(iiii)", &confId,&mosaicId,&comp,&size);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetCompositionType(mosaicId,(Mosaic::Type)comp,size);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetParticipantMosaic(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int mosaicId;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&partId,&mosaicId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetParticipantMosaic(partId,mosaicId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetParticipantSidebar(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int sidebarId;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&partId,&sidebarId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetParticipantSidebar(partId,sidebarId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetMosaicSlot(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int mosaicId;
	int num;
	int id;
	xmlrpc_parse_value(env, param_array, "(iiii)", &confId,&mosaicId,&num,&id);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");
	 
	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Set slot
	int res = conf->SetMosaicSlot(mosaicId,num,id);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* AddMosaicParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int mosaicId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&mosaicId,&partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Set slot
	int res = conf->AddMosaicParticipant(mosaicId,partId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RemoveMosaicParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int mosaicId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&mosaicId,&partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Set slot
	int res = conf->RemoveMosaicParticipant(mosaicId,partId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* AddSidebarParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int sidebarId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&sidebarId,&partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Set slot
	int res = conf->AddSidebarParticipant(sidebarId,partId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* RemoveSidebarParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int sidebarId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&sidebarId,&partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Set slot
	int res = conf->RemoveSidebarParticipant(sidebarId,partId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* CreatePlayer(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;
	UTF8Parser parser;
	
	 //Parseamos
	int confId;
	int privateId;
	char *name;
	xmlrpc_parse_value(env, param_array, "(iis)", &confId,&privateId,&name);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Parse string
	parser.Parse((BYTE*)name,strlen(name));

	//La borramos
	int playerId = conf->CreatePlayer(privateId,parser.GetWString());

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!playerId)
		return xmlerror(env,"Could not create player in conference");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",playerId));
}

xmlrpc_value* DeletePlayer(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int playerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->DeletePlayer(playerId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not delete player from conference");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StartPlaying(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int playerId;
	char *filename;
	int loop;
	xmlrpc_parse_value(env, param_array, "(iisi)", &confId, &playerId, &filename, &loop);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StartPlaying(playerId,filename,loop);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not start playback");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StopPlaying(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int playerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StopPlaying(playerId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not stop play back in conference");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StartRecordingParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int playerId;
	char *filename;
	xmlrpc_parse_value(env, param_array, "(iis)", &confId, &playerId, &filename);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StartRecordingParticipant(playerId,filename);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not start playback");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StopRecordingParticipant(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int playerId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId,&playerId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StopRecordingParticipant(playerId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not stop play back in conference");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SendFPU(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId, &partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SendFPU(partId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not start playback");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetMute(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int media;
	int isMuted;
	xmlrpc_parse_value(env, param_array, "(iiii)", &confId, &partId, &media, &isMuted);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetMute(partId,(MediaFrame::Type)media,isMuted);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Could not start playback");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* GetParticipantStatistics(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	xmlrpc_parse_value(env, param_array, "(ii)", &confId, &partId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//Get statistics
	MultiConf::ParticipantStatistics *partStats = conf->GetParticipantStatistic(partId);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!partStats)
		return xmlerror(env,"Participant not found");

	//Create array
	xmlrpc_value* arr = xmlrpc_array_new(env);
	
	//Process result
	for (MultiConf::ParticipantStatistics::iterator it = partStats->begin(); it!=partStats->end(); ++it)
	{
		//Get media
		std::string media = it->first;
		//Get stats
		MediaStatistics stats = it->second;
		//Create array
		xmlrpc_value* val = xmlrpc_build_value(env,"(siiiiiii)",media.c_str(),stats.isReceiving,stats.isSending,stats.lostRecvPackets,stats.numRecvPackets,stats.numSendPackets,stats.totalRecvBytes,stats.totalSendBytes);
		//Add it
		xmlrpc_array_append_item(env,arr,val);
		//Release
		xmlrpc_DECREF(val);
	}

	//return
	return xmlok(env,arr);
}

xmlrpc_value* MCUEventQueueCreate(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
        MCU *mcu = (MCU *)user_data;

	//Create the event queue
	int queueId = mcu->CreateEventQueue();

	//Si error
	if (!queueId>0)
		return xmlerror(env,"Could not create event queue");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",queueId));
}

xmlrpc_value* MCUEventQueueDelete(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
        MCU *mcu = (MCU *)user_data;

	 //Parseamos
	int queueId;
	xmlrpc_parse_value(env, param_array, "(i)", &queueId);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Delete event queue
	if (!mcu->DeleteEventQueue(queueId))
		return xmlerror(env,"Event queue does not exist");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StartSending(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int media;
	char *sendIp;
	int sendPort;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiisiS)", &confId,&partId,&media,&sendIp,&sendPort,&rtpMap);

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

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

		//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StartSending(partId,(MediaFrame::Type)media,sendIp,sendPort,map);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetRTPProperties(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int media;
	xmlrpc_value *map;
	xmlrpc_parse_value(env, param_array, "(iiiS)", &confId,&partId,&media,&map);

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

		//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetRTPProperties(partId,(MediaFrame::Type)media,properties);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetLocalCryptoSDES(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	//Parseamos
	int confId;
	int partId;
	int media;
	char *suite;
	char *key;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &confId,&partId,&media,&suite,&key);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

		//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetLocalCryptoSDES(partId,(MediaFrame::Type)media,suite,key);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetRemoteCryptoSDES(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	//Parseamos
	int confId;
	int partId;
	int media;
	char *suite;
	char *key;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &confId,&partId,&media,&suite,&key);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

		//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetRemoteCryptoSDES(partId,(MediaFrame::Type)media,suite,key);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetLocalSTUNCredentials(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	//Parseamos
	int confId;
	int partId;
	int media;
	char *username;
	char *pwd;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &confId,&partId,&media,&username,&pwd);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

		//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetLocalSTUNCredentials(partId,(MediaFrame::Type)media,username,pwd);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* SetRemoteSTUNCredentials(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	//Parseamos
	int confId;
	int partId;
	int media;
	char *username;
	char *pwd;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiss)", &confId,&partId,&media,&username,&pwd);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return 0;

		//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->SetRemoteSTUNCredentials(partId,(MediaFrame::Type)media,username,pwd);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StopSending(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int media;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&partId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StopSending(partId,(MediaFrame::Type)media);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error stoping sending video");

	//Devolvemos el resultado
	return xmlok(env);
}

xmlrpc_value* StartReceiving(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int media;
	int recPort = 0;
	xmlrpc_value *rtpMap;
	xmlrpc_parse_value(env, param_array, "(iiiS)", &confId,&partId,&media,&rtpMap);

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

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int recVideoPort = conf->StartReceiving(partId,(MediaFrame::Type)media,map);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!recVideoPort)
		return xmlerror(env,"Could not start receving video");

	//Devolvemos el resultado
	return xmlok(env,xmlrpc_build_value(env,"(i)",recVideoPort));
}

xmlrpc_value* StopReceiving(xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
	MCU *mcu = (MCU *)user_data;
	MultiConf *conf = NULL;

	 //Parseamos
	int confId;
	int partId;
	int media;
	xmlrpc_parse_value(env, param_array, "(iii)", &confId,&partId,&media);

	//Comprobamos si ha habido error
	if(env->fault_occurred)
		return xmlerror(env,"Fault occurred");

	//Obtenemos la referencia
	if(!mcu->GetConferenceRef(confId,&conf))
		return xmlerror(env,"Conference does not exist");

	//La borramos
	int res = conf->StopReceiving(partId,(MediaFrame::Type)media);

	//Liberamos la referencia
	mcu->ReleaseConferenceRef(confId);

	//Salimos
	if(!res)
		return xmlerror(env,"Error stoping receving video");

	//Devolvemos el resultado
	return xmlok(env);
}

XmlHandlerCmd mcuCmdList[] =
{
	{"EventQueueCreate",MCUEventQueueCreate},
	{"EventQueueDelete",MCUEventQueueDelete},
	{"CreateConference",CreateConference},
	{"DeleteConference",DeleteConference},
	{"GetConferences",GetConferences},
	{"CreateMosaic",CreateMosaic},
	{"SetMosaicOverlayImage",SetMosaicOverlayImage},
	{"ResetMosaicOverlay",ResetMosaicOverlay},
	{"DeleteMosaic",DeleteMosaic},
	{"CreateSidebar",CreateSidebar},
	{"DeleteSidebar",DeleteSidebar},
	{"CreateParticipant",CreateParticipant},
	{"DeleteParticipant",DeleteParticipant},
	{"StartBroadcaster",StartBroadcaster},
	{"StopBroadcaster",StopBroadcaster},
	{"StartPublishing",StartPublishing},
	{"StopPublishing",StopPublishing},
	{"StartRecordingParticipant",StartRecordingParticipant},
	{"StopRecordingParticipant",StopRecordingParticipant},
	{"SetVideoCodec",SetVideoCodec},
	{"StartSending",StartSending},
	{"StopSending",StopSending},
	{"StartReceiving",StartReceiving},
	{"StopReceiving",StopReceiving},
	{"SetAudioCodec",SetAudioCodec},
	{"SetTextCodec",SetTextCodec},
	{"SetCompositionType",SetCompositionType},
	{"SetMosaicSlot",SetMosaicSlot},
	{"AddConferenceToken",AddConferenceToken},
	{"AddMosaicParticipant",AddMosaicParticipant},
	{"RemoveMosaicParticipant",RemoveMosaicParticipant},
	{"AddSidebarParticipant",AddSidebarParticipant},
	{"RemoveSidebarParticipant",RemoveSidebarParticipant},
	{"CreatePlayer",CreatePlayer},
	{"DeletePlayer",DeletePlayer},
	{"StartPlaying",StartPlaying},
	{"StopPlaying",StopPlaying},
	{"SendFPU",SendFPU},
	{"SetMute",SetMute},
	{"GetParticipantStatistics",GetParticipantStatistics},
	{"AddParticipantInputToken",AddParticipantInputToken},
	{"AddParticipantOutputToken",AddParticipantOutputToken},
	{"SetParticipantMosaic",SetParticipantMosaic},
	{"SetParticipantSidebar",SetParticipantSidebar},
	{"SetRemoteCryptoSDES",SetRemoteCryptoSDES},
	{"SetLocalCryptoSDES",SetLocalCryptoSDES},
	{"SetLocalSTUNCredentials",SetLocalSTUNCredentials},
	{"SetRemoteSTUNCredentials",SetRemoteSTUNCredentials},
	{"SetRTPProperties",SetRTPProperties},
	{NULL,NULL}
};
