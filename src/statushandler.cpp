#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include "statushandler.h"

#define STATUS "System is running\n"

/**************************************
* ProcessRequest
*	Procesa una peticion
*************************************/
int StatusHandler::ProcessRequest(TRequestInfo *req,TSession * const ses)
{

	Log(">ProcessRequest [%s]\n",req->uri);

	int inputLen = 0;
	char * buffer = NULL;

	//Obtenemos el content length
	const char * content_length = RequestHeaderValue(ses, (char*)"content-length");

	//Si no hay 
	if (content_length != NULL)
	{
		//Obtenemos el entero
		inputLen = atoi(content_length);

		//Creamos un buffer para el body
		buffer = (char *) malloc(inputLen);
	}

	//Check lenght
	if (inputLen)
	{
		if (!XmlRpcServer::GetBody(ses,buffer,inputLen))
		{
			//LIberamos el buffer
			free(buffer);

			//Y salimos sin devolver nada
			return Error("Error getting request body\n");
		}
	}

	//POnemos el content type
	ResponseContentType(ses, (char*)"text/html;");

	//Y escribimos
	XmlRpcServer::SendResponse(ses,200,STATUS,strlen(STATUS));

	//Liberamos el buffer
	if (buffer != NULL)
		//Free buffer
		free(buffer);

	Log("<ProccessRequest\n");

	return 1;
}

