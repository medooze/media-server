#ifndef _XMLRPCMCUCLIENT_H_
#define _XMLRPCMCUCLIENT_H_

#ifdef __cplusplus

#include "xmlrpcclient.h"
#include <string>
using namespace std;

class XmlRpcMcuClient
{
public:
	XmlRpcMcuClient(char * url);
	~XmlRpcMcuClient();
	
	int CreateConference(char *name);
	int CreateParticipant(int confId);
	int SetCompositionType(int confId,int comp,int size);
	int SetMosaicSlot(int confId,int num,int id);

	//Video
	int SetVideoCodec(int confId,int partId,int codec,int mode,int fps,int bitrate,int quality=0, int fillLevel=0);
	int StartSendingVideo(int confId,int partId,char *sendVideoIp,int sendVideoPort);
	int StopSendingVideo(int confId,int partId);
	int StartReceivingVideo(int confId,int partId,int *recVideoPort);
	int StopReceivingVideo(int confId,int partId);
	int IsSendingVideo(int confId,int partId,int *isSending);
	int IsReceivingVideo(int confId,int partId,int *isReceiving);

	//Audio
	int SetAudioCodec(int confId,int partId,int codec);
	int StartSendingAudio(int confId,int partId,char *sendAudioIp,int sendAudioPort);
	int StopSendingAudio(int confId,int partId);
	int StartReceivingAudio(int confId,int partId,int *recAudioPort);
	int StopReceivingAudio(int confId,int partId);
	int IsSendingAudio(int confId,int partId,int *isSending);
	int IsReceivingAudio(int confId,int partId,int *isReceiving);
	
	int DeleteParticipant(int confId,int partId);
	int DeleteConference(int confId);

private:
	int MakeCall(xmlrpc_env *env,const char *method,xmlrpc_value* param,xmlrpc_value **result);
	//Atributos
	string serverUrl;
	XmlRpcClient rpc;
};

extern "C" 
{

void * 	CreateMCUClient(char *url)
	{ return new XmlRpcMcuClient(url);	}
void	DestroyMCUClient(void *mcu)
	{ delete (XmlRpcMcuClient*)mcu;		}	
	
int 	CreateConference(void *mcu,char *name)
	{ return ((XmlRpcMcuClient*)mcu)->CreateConference(name);		}
int 	CreateParticipant(void *mcu,int confId)
	{ return ((XmlRpcMcuClient*)mcu)->CreateParticipant(confId);		}
int 	SetCompositionType(void *mcu,int confId,int comp,int size)
	{ return ((XmlRpcMcuClient*)mcu)->SetCompositionType(confId,comp,size);	}
int 	SetMosaicSlot(void *mcu,int confId,int num,int id)
	{ return ((XmlRpcMcuClient*)mcu)->SetCompositionType(confId,num,id);	}

//Video
int 	SetVideoCodec(void *mcu,int confId,int partId,int codec,int mode,int fps,int bitrate)
	{ return ((XmlRpcMcuClient*)mcu)->SetVideoCodec(confId,partId,codec,mode,fps,bitrate);		} 
int 	StartSendingVideo(void *mcu,int confId,int partId,char *sendVideoIp,int sendVideoPort)
	{ return ((XmlRpcMcuClient*)mcu)->StartSendingVideo(confId,partId,sendVideoIp,sendVideoPort); 	}
int 	StopSendingVideo(void *mcu,int confId,int partId)
	{ return ((XmlRpcMcuClient*)mcu)->StopSendingVideo(confId,partId);				}
int 	StartReceivingVideo(void *mcu,int confId,int partId,int *recVideoPort)
	{ return ((XmlRpcMcuClient*)mcu)->StartReceivingVideo(confId,partId,recVideoPort);		}
int 	StopReceivingVideo(void *mcu,int confId,int partId)
	{ return ((XmlRpcMcuClient*)mcu)->StopReceivingVideo(confId,partId);				}
int 	IsSendingVideo(void *mcu,int confId,int partId,int *isSending)
	{ return ((XmlRpcMcuClient*)mcu)->IsSendingVideo(confId,partId,isSending);			}
int 	IsReceivingVideo(void *mcu,int confId,int partId,int *isReceiving)
	{ return ((XmlRpcMcuClient*)mcu)->IsReceivingVideo(confId,partId,isReceiving);			}

//Audio
int 	SetAudioCodec(void *mcu,int confId,int partId,int codec)
	{ return ((XmlRpcMcuClient*)mcu)->SetAudioCodec(confId,partId,codec);				}
int 	StartSendingAudio(void *mcu,int confId,int partId,char *sendAudioIp,int sendAudioPort)
	{ return ((XmlRpcMcuClient*)mcu)->StartSendingAudio(confId,partId,sendAudioIp,sendAudioPort);	}
int 	StopSendingAudio(void *mcu,int confId,int partId)
	{ return ((XmlRpcMcuClient*)mcu)->StopSendingAudio(confId,partId);				}
int 	StartReceivingAudio(void *mcu,int confId,int partId,int *recAudioPort)
	{ return ((XmlRpcMcuClient*)mcu)->StartReceivingAudio(confId,partId,recAudioPort);		}
int 	StopReceivingAudio(void *mcu,int confId,int partId)
	{ return ((XmlRpcMcuClient*)mcu)->StopReceivingAudio(confId,partId);				}
int 	IsSendingAudio(void *mcu,int confId,int partId,int *isSending)
	{ return ((XmlRpcMcuClient*)mcu)->IsSendingAudio(confId,partId,isSending);			}
int 	IsReceivingAudio(void *mcu,int confId,int partId,int *isReceiving)
	{ return ((XmlRpcMcuClient*)mcu)->IsReceivingAudio(confId,partId,isReceiving);			}

int 	DeleteParticipant(void *mcu,int confId,int partId)
	{ return ((XmlRpcMcuClient*)mcu)->DeleteParticipant(confId,partId);				}
int 	DeleteConference(void *mcu,int confId)
	{ return ((XmlRpcMcuClient*)mcu)->DeleteConference(confId);					}
}
#else

void * 	CreateMCUClient(char *url);
void	DestroyMCUClient(void *mcu);
	
int 	CreateConference(void *mcu,char *name);
int 	CreateParticipant(void *mcu,int confId);
int 	SetCompositionType(void *mcu,int confId,int comp,int size);
int 	SetMosaicSlot(void *mcu,int confId,int num,int id);

//Video
int 	SetVideoCodec(void *mcu,int confId,int partId,int codec,int mode,int fps,int bitrate);
int 	StartSendingVideo(void *mcu,int confId,int partId,char *sendVideoIp,int sendVideoPort);
int 	StopSendingVideo(void *mcu,int confId,int partId);
int 	StartReceivingVideo(void *mcu,int confId,int partId,int *recVideoPort);
int 	StopReceivingVideo(void *mcu,int confId,int partId);
int 	IsSendingVideo(void *mcu,int confId,int partId,int *isSending);
int 	IsReceivingVideo(void *mcu,int confId,int partId,int *isReceiving);

//Audio
int 	SetAudioCodec(void *mcu,int confId,int partId,int codec);
int 	StartSendingAudio(void *mcu,int confId,int partId,char *sendAudioIp,int sendAudioPort);
int 	StopSendingAudio(void *mcu,int confId,int partId);
int 	StartReceivingAudio(void *mcu,int confId,int partId,int *recAudioPort);
int 	StopReceivingAudio(void *mcu,int confId,int partId);
int 	IsSendingAudio(void *mcu,int confId,int partId,int *isSending);
int 	IsReceivingAudio(void *mcu,int confId,int partId,int *isReceiving);
int 	DeleteParticipant(void *mcu,int confId,int partId);
int 	DeleteConference(void *mcu,int confId);


#endif //__cplusplus

#endif

