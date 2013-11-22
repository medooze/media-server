#include <stdio.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include "log.h"
#include "xmlrpcclient.h"

/****************************
* Init
*	Global Init
****************************/
int XmlRpcClient::Init()
{
	return curl_global_init(CURL_GLOBAL_ALL);
}

/****************************
* End
*	Global cleaning
******************************/
int XmlRpcClient::End()
{
	int curl_global_cleanup();
}

/***************************************
* collect
*	This is a Curl output function.  Curl calls this to deliver the
*       HTTP response body.  Curl thinks it's writing to a POSIX stream.
************************************/
static size_t collect(void *  const ptr, size_t  const size, size_t  const nmemb, FILE  * const stream) 
{
	xmlrpc_env env;
	xmlrpc_env_init(&env);
	size_t const length = nmemb * size;

	//A�adimos
	xmlrpc_mem_block_append(&env, (xmlrpc_mem_block *) stream, (char *)ptr,length);

	//Si error
	if (env.fault_occurred)
		return 	(size_t)-1;

	xmlrpc_env_clean(&env);

	//Salimos
	return length;
}

int XmlRpcClient::MakeCall(const char *uri,const char *method,xmlrpc_value *params,xmlrpc_value **result)
{
	CURLcode 		res;
	CURL* 			curlSession = NULL;
	curl_slist* 		headerList = NULL;
	xmlrpc_env 		env;
	xmlrpc_mem_block* 	callXml;
	xmlrpc_mem_block* 	responseXml;
	char curlError[CURL_ERROR_SIZE];
	int error = false;

	//Creamos la session
	curlSession = curl_easy_init();

	//Si error
	if(!curlSession)
		return Error("No se puede crear la sesion");

	//Iniciamos el env
	xmlrpc_env_init(&env);

	//Creamos las cabeceras
	headerList = curl_slist_append(headerList, "Content-Type: text/xml");

	//Los bloques de memoria
	callXml = xmlrpc_mem_block_new(&env,0);
	responseXml = xmlrpc_mem_block_new(&env,0);

	//Serializazmos la llamada
	xmlrpc_serialize_call(&env, callXml, method, params);

	//Si ha habido error
	if (env.fault_occurred) 
	{
		//Error
		error = Error("Error con los parametros");
		//Limpiamos
		goto cleanup;
	}

	//A�adimos el \0 del final
	xmlrpc_mem_block_append(&env, callXml,(void *)"\0", 1);

	//Set up enviroment
	curl_easy_setopt(curlSession, CURLOPT_HTTPHEADER,headerList);
	curl_easy_setopt(curlSession, CURLOPT_POST, 1 );
	curl_easy_setopt(curlSession, CURLOPT_URL, uri);
	curl_easy_setopt(curlSession, CURLOPT_POSTFIELDS, XMLRPC_MEMBLOCK_CONTENTS(char, callXml));
	curl_easy_setopt(curlSession, CURLOPT_FILE, responseXml);
	curl_easy_setopt(curlSession, CURLOPT_HEADER, 0 );
	curl_easy_setopt(curlSession, CURLOPT_WRITEFUNCTION, collect);
	curl_easy_setopt(curlSession, CURLOPT_ERRORBUFFER,curlError);
	curl_easy_setopt(curlSession, CURLOPT_NOPROGRESS, 1);

	//Hacemos la peticion
 	res = curl_easy_perform(curlSession);

	//Si error
	if (res != CURLE_OK)
	{
		//Error
		error = Error("%s",curlError);
		//Limpiamos
		goto cleanup;
	}

	//Ejecutamos
	long http_result;
	res = curl_easy_getinfo(curlSession, CURLINFO_HTTP_CODE, &http_result);


	//Si ha salido bien
	if (res != CURLE_OK)
	{
		//Error
		error = Error("%s\n",curlError);
		//Limpiamos
		goto cleanup;
	}

	//Comprobamos el codig de respuesta
	if (http_result != 200)
	{
		//Error
		error = Error("Http code %d\n",http_result);
		//Limpiamos
		goto cleanup;
	}

	//Parseamos la respuesta
	*result = xmlrpc_parse_response( &env, 
			XMLRPC_MEMBLOCK_CONTENTS(char, responseXml), 
			XMLRPC_MEMBLOCK_SIZE(char, responseXml));

cleanup:
	//Limpiamos
	curl_slist_free_all(headerList);
	curl_easy_cleanup(curlSession);

	//Liberamos el bloque
	xmlrpc_mem_block_free(responseXml);

	xmlrpc_env_clean(&env);

	//Salimos
	return !error;

}

