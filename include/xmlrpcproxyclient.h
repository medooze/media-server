#ifndef _XMLRPCPROXYCLIENT_H_
#define _XMLRPCPROXYCLIENT_H_
#include <xmlrpc.h>
#include <xmlrpc_client.h>

class MultiConf
{
public:
	MultiConf(string uri);
	~MultiConf();

	int CreateConference(char *name);
	int InitConference(int confId);
	int CreateParticipant(int confId);
	int SetCompositionType(int confId,int comp,int size);

	//Video
	int SetVideoCodec(int confId,int partId,int codec,int mode,int fps,int bitrate,int quality=0, int fillLevel=0);
	int StartSendingVideo(int confId,int partId,char *sendVideoIp,int sendVideoPort);
	int StopSendingVideo(int confId,int partId);
	int StartReceivingVideo(int confId,int partId,int recVideoPort);
	int StopReceivingVideo(int confId,int partId);
	int IsSendingVideo(int confId,int partId);
	int IsReceivingVideo(int confId,int partId);

	//Audio
	int SetAudioCodec(int confId,int partId);
	int StartSendingAudio(int confId,int partId,char *sendAudioIp,int sendAudioPort);
	int StopSendingAudio(int confId,int partId);
	int StartReceivingAudio(int confId,int partId,int recAudioPort);
	int StopReceivingAudio(int confId,int partId);
	int IsSendingAudio(int confId,int partId);
	int IsReceivingAudio(int confId,int partId);
	
	int DeleteParticipant(int confId,int partId);
	int EndConference(int confId);

private:
	//Atributos
	XmlRpcClient	client;
	
};
#endif
