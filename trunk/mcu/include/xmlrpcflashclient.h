#ifndef _XMLRPCFLASHLIENT_H_
#define _XMLRPCFLASHLIENT_H_

#ifdef __cplusplus

#include "xmlrpcclient.h"
#include <string>
using namespace std;

class XmlRpcFlashClient
{
public:
	XmlRpcFlashClient(char * url);
	~XmlRpcFlashClient();
	
	int StartPlaying(char *ip, int audioPort, int videoPort, char* url);
	int StopPlaying(int id);
private:
	int MakeCall(xmlrpc_env *env,char *method,xmlrpc_value* param,xmlrpc_value **result);
	//Atributos
	string serverUrl;
	XmlRpcClient rpc;
};

extern "C" 
{

void * 	CreateFlashClient(char *url)
	{ return new XmlRpcFlashClient(url);	}
void	DestroyFlashClient(void *flash)
	{ delete (XmlRpcFlashClient*)flash;		}	
	
int 	FlashStartPlaying(void *flash,char *ip, int audioPort, int videoPort, char* url)
	{ return ((XmlRpcFlashClient*)flash)->StartPlaying(ip,audioPort,videoPort,url);	}
int 	FlashStopPlaying(void *flash,int id)
	{ return ((XmlRpcFlashClient*)flash)->StopPlaying(id);		}
}
#else

void * 	CreateFlashClient(char *url);
void	DestroyFlashClient(void *mcu);
	
int 	FlashStartPlaying(void *flash,char *ip, int audioPort, int videoPort, char* url);
int 	FlashStopPlaying(void *flash,int id);

#endif //__cplusplus

#endif

