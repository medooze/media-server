#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include <xmlrpc-c/abyss.h>
#include "xmlrpcserver.h"

#define ERRORMSG "MCU Error."
#define SHUTDOWNMSG "Shutting down."

/**************************************
* XmlRpcServer
*	Constructor
**************************************/
XmlRpcServer::XmlRpcServer(int port)
{
	char name[65];

	//Iniciamos la fecha
	DateInit();

	//Los mime tipes
	MIMETypeInit();

	//Le pasamos como nombre un puntero a nosotros mismos
	sprintf(name,"%p",this);	

	//Creamos el servidor
	ServerCreate(&srv,name, port, DEFAULT_DOCS, "http.log");

	//Iniciamos el servidor
	ServerInit(&srv);

	//Set the handler
	abyss_bool ret;

	//Create abyss handler
	ServerReqHandler3 abbysHndlr;

	//Set
	abbysHndlr.userdata	= (void*)this;
	abbysHndlr.handleReq	= RequestHandler;
	abbysHndlr.term		= NULL;
	abbysHndlr.handleReqStackSize = 0;

	//Add handler
	ServerAddHandler3(&srv,&abbysHndlr,&ret);
}

/**************************************
* Start
*	Arranca el servidor
**************************************/
int XmlRpcServer::Start()
{
	Log("-Start [%p]\n",this);

	//Start it
	running = 1;

	//And run
	Run();
	
	return 1;
}

/**************************************
* XmlRpcServer
*	Constructor
**************************************/
int XmlRpcServer::Run()
{
	Log(">Run [%p]\n",this);

	//Vamos a buscar en orden inverso
	LstHandlers::reverse_iterator it;

	//Recorremos la lista
	for (it=lstHandlers.rbegin();it!=lstHandlers.rend();it++)	
		Log("-Handler on %s\n",(*it).first.c_str());

	//Ejecutamos
	ServerRun(&srv);

	Log("<Run\n");

	return 1;
}

/**************************************
* Stop
*	Para el servidor
**************************************/
int XmlRpcServer::Stop()
{
	Log("-Stop [%p]\n",this);

	running = 0;

	//Stop sercer
	ServerTerminate(&srv);
	
	return 1;
}
/**************************************
* AddHandler 
*	A�ade un handler para una uri
**************************************/
int XmlRpcServer::AddHandler(string base,Handler* hnd)
{
	//A�adimos al map
	lstHandlers[base] = hnd;

	return 1;
}

/**************************************
* RequestHandler
*	Callback
**************************************/
void XmlRpcServer::RequestHandler(void *par,TSession *ses, abyss_bool *ret)
{
	//Obtenemos el servidor
	XmlRpcServer *serv = (XmlRpcServer *)par;

	//Procesamos la llamada
	*ret = serv->DispatchRequest(ses);
}

/**************************************
* DispatchRequest
*       Busca el handler para procesar la peticion
**************************************/
int XmlRpcServer::DispatchRequest(TSession *ses)
{
	TRequestInfo *req;

	//Get request info
	SessionGetRequestInfo(ses,(const TRequestInfo**)&req);

	//Log it
	Log("-Dispatching [%s]\n",req->uri);


	//Obtenemos la uri
	string uri = req->uri;

	//Vamos a buscar en orden inverso
	LstHandlers::reverse_iterator it;

	//Check stop
	if (uri.find("/stop")==0)
	{
		//Stop
		Stop();
		//Devolvemos el error
		SendResponse(ses,200,SHUTDOWNMSG,strlen(SHUTDOWNMSG));
		//Exit
		return 1;
	}
	
	//Recorremos la lista
	for (it=lstHandlers.rbegin();it!=lstHandlers.rend();it++)	
	{
		//Si la uri empieza por la base del handler
		if (uri.find((*it).first)==0)
			//Ejecutamos el handler
			return (*it).second->ProcessRequest(req,ses);
	}

	//Devolvemos el error
	SendError(ses,404);

	//Exit
	return 1;
}



/**************************************
* GetBody
	Devuelve el body de una peticion
**************************************/
int XmlRpcServer::GetBody(TSession *ses,char *body,short bodyLen)
{
	int len=0;

	//MIentras no hayamos leido del todo
	while (len<bodyLen)
	{
		char * buffer;
		size_t readed;

		//If there is no data available
		if (!SessionReadDataAvail(ses))
			//Refill buffer
			SessionRefillBuffer(ses);

		//Read data
		SessionGetReadData(ses,bodyLen-len,(const char**)&buffer,&readed);

		//If not readed
		if (!readed)
			//error
			return Error("Not enought data readed");
		//Copy
		memcpy(body+len,buffer,readed);

		//Increased readed
		len+=readed;
	}

	//Return
	return len;
/*/

	//Obtenemos lo que quedaba en el buffer
	ConnReadInit(r->conn);

	//Obtenemos lo que hay en la conexion
	if (r->conn->buffersize > bodyLen)
		len = bodyLen;
	else
		len = r->conn->buffersize;

	//Copiamos
	memcpy(body,r->conn->buffer,len);

	//MIentras no hayamos leido del todo
	while (len<bodyLen)
	{
		int size;

		//Iniciamos la lectura
		ConnReadInit(r->conn);

		//Leemos
		if(!ConnRead(r->conn,100))
			return 0;

		//Obtenemos lo que hay en la conexion
		if (r->conn->buffersize > bodyLen - len)
			size = bodyLen - len;
		else
			size = r->conn->buffersize;

		//Copiamos
		memcpy(body+len,r->conn->buffer,size);

		//Incrementamos el buffer
		len += size;

		//Increase buffer pos
		r->conn->bufferpos += len;
	}

	//Reset buffer
	r->conn->buffersize = 0;
	r->conn->bufferpos = 0;

	//Clean buffer
	ConnReadInit(r->conn);

	//Salimos bien
	return 1;
 * */
}

/**************************************
* SendResponse
*	Envia la respuesta de una peticion
**************************************/
int XmlRpcServer::SendResponse(TSession *r, short code, const char *msg, int length)
{
	//Chunked output
	ResponseChunked(r);

	//POnemos el codigo
	ResponseStatus(r,code);

	//El content length
	ResponseContentLength(r, length);

	//Escribimos la respuesta
	ResponseWriteStart(r);
	
	//La mandamos
	ResponseWriteBody(r,(char*)msg,length);

	//End it
	ResponseWriteEnd(r);

	return 1;
}

/**************************************
* SendError
*	Devuelve el html con el error
**************************************/
int XmlRpcServer::SendError(TSession * r, short code) 
{
	//POnemos el content type
	ResponseContentType(r, (char*)"text/html; charset=\"utf-8\"");

	//Escribimos el codigo de error
	return SendResponse(r,code,(char*)ERRORMSG,strlen(ERRORMSG));
}

/**************************************
* SendError
*	Devuelve el html con el error
**************************************/
int XmlRpcServer::SendError(TSession * r, short code,const char *msg)
{
	//POnemos el content type
	ResponseContentType(r, (char*)"text/html; charset=\"utf-8\"");

	//Escribimos el codigo de error
	return SendResponse(r,code,(char*)msg,strlen(msg));
}
