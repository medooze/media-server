#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "log.h"
#include "rtpparticipant.h"
#include "multiconf.h"
#include "rtmp/rtmpparticipant.h"


/************************
* MultiConf
* 	Constructor
*************************/
MultiConf::MultiConf(const std::wstring &tag) : 
	broadcast(tag),
	videoMixer(tag),
	chat(tag),
	flvEncoder(0),
	appMixerEncoder(1)
{
	//Guardamos el nombre
	this->tag = tag;

	//No watcher or broadcaster
	watcherId = 0;
	broadcastId = 0;

	//Init counter
	maxPublisherId = 1;

	//Inicializamos el contador
	maxId = 500;

	//Y no tamos iniciados
	inited = 0;

        //No recorder
        recorder = NULL;
	appMixerBroadcastEnabled = false;
	appMixerEncoderPrivateMosaicId = 0;
	
	//Nothing else
	listener = NULL;
	param = NULL;


}

/************************
* ~ MultiConf
* 	Destructor
*************************/
MultiConf::~MultiConf()
{
	
	//Pa porsi
	if (inited)
		End();
        //Delete recoreder
        if (recorder)
                //Delete
                delete(recorder);
}

void MultiConf::SetListener(Listener *listener,void* param)
{
	//Store values
	this->listener = listener;
	this->param = param;
}

/************************
* Init
* 	Constructo
*************************/
int MultiConf::Init(const Properties &properties)
{
	//Get vad and rate properties
	VideoMixer::VADMode vad = (VideoMixer::VADMode) properties.GetProperty("vad-mode",0);
	
	Log("-Init multiconf [vad:%d]\n",vad);

	//We are inited
	inited = true;
	
	//Check if we need to use vad
	if (vad)
	{
		//Set vad mode
		videoMixer.SetVADMode(vad);
		//Calculate vad
		audioMixer.SetCalculateVAD(true);
		//Set VAD proxy
		videoMixer.SetVADProxy(&audioMixer);
	}
	//Init audio mixers
	int res = audioMixer.Init(properties.GetChildren("audio.mixer"));
	//Init video mixer with dedault parameters
	res &= videoMixer.Init(properties.GetChildren("video.mixer"));
	//Init text mixer
	res &= textMixer.Init();

	//Check if we are inited
	if (!res)
		//End us
		End();

	//Get the id
	watcherId = maxId++;

	//Create audio and text watcher
	audioMixer.CreateMixer(watcherId);
	std::wstring name = std::wstring(L"watcher");
	textMixer.CreateMixer(watcherId,name);

	//Init the audio encoder
	audioEncoder.Init(audioMixer.GetInput(watcherId));
	//Init the text encoder
	textEncoder.Init(textMixer.GetInput(watcherId));

	//Set codec
	audioEncoder.SetAudioCodec(AudioCodec::PCMA,properties.GetChildren("mixer.audio.encoder"));

	//Start mixers
	audioMixer.InitMixer(watcherId,0);
	textMixer.InitMixer(watcherId);

	//Start encoding
	audioEncoder.StartEncoding();
	textEncoder.StartEncoding();

	//Create one mixer for the app Mixer ouuput
	videoMixer.CreateMixer(AppMixerId,std::wstring(L"AppMixer"));

	//Init
	appMixer.Init(videoMixer.GetOutput(AppMixerId));

	//Init mixer for the app mixer
	videoMixer.InitMixer(AppMixerId,-1);

	
	//Start chat
	chat.Init();

	return res;
}

/************************
* SetCompositionType
* 	Set mosaic type and size
*************************/
int MultiConf::SetCompositionType(int mosaicId,Mosaic::Type comp,int size)
{
	Log("-SetCompositionType [mosaic:%d,comp:%d,size:%d]\n",mosaicId,comp,size);

	//POnemos el tipo de video mixer
	return videoMixer.SetCompositionType(mosaicId,comp,size);
}

/*****************************
* StartBroadcaster
* 	Create FLV Watcher port
*******************************/
int MultiConf::StartBroadcaster(const Properties &properties)
{
	std::wstring name = std::wstring(L"broadcaster");

	Log(">StartBroadcaster\n");

	//Cehck if running
	if (!inited)
		//Exit
		return Error("-Not inited\n");
	
	//Lock it
	broacasterLock.WaitUnusedAndLock();

	//Check we don't have one already
	if (broadcastId>0)
	{
		//Unlcok
		broacasterLock.Unlock();
		//Error
		return Error("We already have one broadaster");
	}
	
	//Get the id
	broadcastId = maxId++;

	//Create mixers
	videoMixer.CreateMixer(broadcastId,name);
	audioMixer.CreateMixer(broadcastId);
	textMixer.CreateMixer(broadcastId,name);

	//Create the broadcast session without limits
	broadcast.Init(0,0);

	//Init it flv encoder
	flvEncoder.Init(audioMixer.GetInput(broadcastId),videoMixer.GetInput(broadcastId),properties.GetChildren("broadcast"));

	//Init mixers
	videoMixer.InitMixer(broadcastId,0);
	audioMixer.InitMixer(broadcastId,0);
	textMixer.InitMixer(broadcastId);

	//Start encoding
	flvEncoder.StartEncoding();
	
	//If we are broadcasting the appmixer as a separate stream
	if (properties.HasProperty("broadcast.appmixer.enabled"))
	{
		Log("-Enabling appmixer broadcast\n");
		
		//Enable
		appMixerBroadcastEnabled = true;
		//Get size
		DWORD size = properties.GetProperty("broadcast.appmixer.size",HD720P);
		
		//Create private mosaic 
		appMixerEncoderPrivateMosaicId = videoMixer.CreateMosaic(Mosaic::mosaic1x1,size);
		//Add app mixer to it
		videoMixer.AddMosaicParticipant(appMixerEncoderPrivateMosaicId,AppMixerId);
		//Display the app mixer on if
		videoMixer.SetSlot(appMixerEncoderPrivateMosaicId,0,AppMixerId);
		
		//Create mixers for broadcaster
		videoMixer.CreateMixer(AppMixerBroadcasterId,L"appMixerBroadcast");
		//Init the flv encoder for the app mixer
		appMixerEncoder.Init(NULL,videoMixer.GetInput(AppMixerBroadcasterId),properties.GetChildren("broadcast.appmixer"));
		//Init mixers
		videoMixer.InitMixer(AppMixerBroadcasterId,appMixerEncoderPrivateMosaicId);
		
		//Start encoding
		appMixerEncoder.StartEncoding();
	}
	
	//Unlcok
	broacasterLock.Unlock();

	Log("<StartBroadcaster\n");

	return 1;
}

int MultiConf::StartRecordingBroadcaster(const char* filename)
{
	//Log filename
	Log("-Recording broadcast [file:\"%s\"]\n",filename);

	//Find last "."
	const char* ext = (const char*)strrchr(filename,'.');

	//If not found
	if (!ext)
		//Error
		return Error("Extension not found for [file:\"%s\"]\n",filename);

	//Lock it
	broacasterLock.WaitUnusedAndLock();
	
	//Check file name
	if (strncasecmp(ext,".flv",4)==0) {
		//FLV
		recorder = new FLVRecorder();
	} else if (strncasecmp(ext,".mp4",4)==0) {
		//MP4
		recorder = new MP4Recorder();
	} else {
		//Unlcok
		broacasterLock.Unlock();
		//Error
		return Error("Not known extension [ext:\"%s\"]\n",ext);
	}

	//Open file for recording
	if (!recorder->Create(filename))
		//Fail
		goto error;

	//And start recording
	if (!recorder->Record())
		//Fail
		goto error;

	//Check type
	switch (recorder->GetType())
	{
		case RecorderControl::FLV:
			//Set RTMP listeners
			flvEncoder.AddMediaListener((FLVRecorder*)recorder);
			if (appMixerBroadcastEnabled)
				appMixerEncoder.AddMediaListener((FLVRecorder*)recorder);
			break;
		case RecorderControl::MP4:
			//Set RTMP listener
			flvEncoder.AddMediaFrameListener((MP4Recorder*)recorder);
			if (appMixerBroadcastEnabled)
				appMixerEncoder.AddMediaFrameListener((MP4Recorder*)recorder);
			break;
	}

	//Unlcok
	broacasterLock.Unlock();
	
	//OK
	return 1;
error:
	
	//Delete recorder
	delete(recorder);
	//NUL
	recorder = NULL;
	//Unlcok
	broacasterLock.Unlock();
	//Error
	return 0;
	
}

int MultiConf::StopRecordingBroadcaster()
{
	//Lock it
	broacasterLock.WaitUnusedAndLock();
	
	//Check recording
	if (!recorder)
	{
		//Unlcok
		broacasterLock.Unlock();
		//Error
		return Error("Recorder not started");
	}
	
	//Check type
	switch (recorder->GetType())
	{
		case RecorderControl::FLV:
			//Set RTMP listener
			flvEncoder.RemoveMediaListener((FLVRecorder*)recorder);
			if (appMixerBroadcastEnabled)
				flvEncoder.RemoveMediaListener((FLVRecorder*)recorder);
			break;
		case RecorderControl::MP4:
			//Set RTMP listener
			flvEncoder.RemoveMediaFrameListener((MP4Recorder*)recorder);
			if (appMixerBroadcastEnabled)
				appMixerEncoder.RemoveMediaFrameListener((MP4Recorder*)recorder);
			break;
	}

	//Close recorder
	recorder->Stop();

	//Close recorder
	recorder->Close();
	
	//Delete it
	delete(recorder);
	
	//And set to null
	recorder = NULL;
	
	//Unlcok
	broacasterLock.Unlock();

	//Exit
	return 1;
}

/*****************************
* StopBroadcaster
*       End FLV Watcher port
******************************/
int MultiConf::StopBroadcaster()
{
	//If no watcher
	if (!broadcastId)
		//exit
		return 0;

	Log(">StopBroadcaster\n");

	//Check recorder
	StopRecordingBroadcaster();
	
	//Lock it
	broacasterLock.WaitUnusedAndLock();

	Log("-flvEncoder.StopEncoding\n");
	//Stop endoding
	flvEncoder.StopEncoding();

	Log("-Ending mixers\n");
	//End mixers
	videoMixer.EndMixer(broadcastId);
	audioMixer.EndMixer(broadcastId);
	textMixer.EndMixer(broadcastId);
		
	Log("flvEncoder.End\n");
	//End Transmiter
	flvEncoder.End();
		
	Log("Ending publishers");
	//For each publisher
	Publishers::iterator it = publishers.begin();

	//until last
	while (it!=publishers.end())
	{
		//Get first publisher info
		PublisherInfo info = it->second;
		//Check stream
		if (info.stream)
		{
			//Un publish
			info.stream->UnPublish();
			//And close
			info.stream->Close();
			//Remove listener
			flvEncoder.RemoveMediaListener((RTMPMediaStream::Listener*)info.stream);
			//Delete it
			delete(info.stream);
		}
		//Disconnect
		info.conn->Disconnect();
		//Delete connection
		delete(info.conn);
		//Remove
		publishers.erase(it++);
	}

	//Stop broacast
	broadcast.End();

	//End mixers
	videoMixer.DeleteMixer(broadcastId);
	audioMixer.DeleteMixer(broadcastId);
	textMixer.DeleteMixer(broadcastId);
	
	//Unset watcher id
	broadcastId = 0;
	
	//Stop app mixer broadcaster
	if (appMixerBroadcastEnabled)
	{
		//Stop encofing
		appMixerEncoder.StopEncoding();
		//End mixer
		videoMixer.EndMixer(AppMixerBroadcasterId);
		//End encoder
		appMixerEncoder.End();
		//And delete mixer
		videoMixer.DeleteMixer(AppMixerBroadcasterId);
		//Delete private mosaic
		videoMixer.DeleteMosaic(appMixerEncoderPrivateMosaicId);
		//And reset
		appMixerEncoderPrivateMosaicId = 0;
		appMixerBroadcastEnabled = false;
	}
	
	//Unlcok
	broacasterLock.Unlock();

	Log("<StopBroadcaster\n");

	return 1;
}

/************************
* SetMosaicSlot
* 	Set slot position on mosaic
*************************/
int MultiConf::SetMosaicSlot(int mosaicId,int slot,int id)
{
	Log("-SetMosaicSlot [mosaic:%d,slot:%d,id:%d]\n",mosaicId,slot,id);

	//Set it
	return videoMixer.SetSlot(mosaicId,slot,id);
}

/************************
* ResetMosaicSlots
* 	Free all mosaic slots
*************************/
int MultiConf::ResetMosaicSlots(int mosaicId)
{
	Log("-ResetMosaicSlots [mosaic:%d]\n",mosaicId);

	//Set it
	return videoMixer.ResetSlots(mosaicId);
}

int MultiConf::GetMosaicPositions(int mosaicId,std::list<int> &positions)
{
	//Set it
	return videoMixer.GetMosaicPositions(mosaicId,positions);
}

/************************
* AddMosaicParticipant
* 	Show participant in a mosaic
*************************/
int MultiConf::AddMosaicParticipant(int mosaicId,int partId)
{
	//Set it
	return videoMixer.AddMosaicParticipant(mosaicId,partId);
}

/************************
* RemoveMosaicParticipant
* 	Unshow a participant in a mosaic
*************************/
int MultiConf::RemoveMosaicParticipant(int mosaicId,int partId)
{
	Log("-RemoveMosaicParticipant [mosaic:%d,partId:%d]\n",mosaicId,partId);

	//Set it
	return videoMixer.RemoveMosaicParticipant(mosaicId,partId);
}


/************************
* AddSidebarParticipant
* 	Show participant in a Sidebar
*************************/
int MultiConf::AddSidebarParticipant(int sidebarId,int partId)
{
	//Set it
	return audioMixer.AddSidebarParticipant(sidebarId,partId);
}

/************************
* RemoveSidebarParticipant
* 	Unshow a participant in a Sidebar
*************************/
int MultiConf::RemoveSidebarParticipant(int sidebarId,int partId)
{
	Log("-RemoveSidebarParticipant [sidebar:%d,partId:]\n",sidebarId,partId);

	//Set it
	return audioMixer.RemoveSidebarParticipant(sidebarId,partId);
}

/************************
* CreateMosaic
* 	Add a mosaic to the conference
*************************/
int MultiConf::CreateMosaic(Mosaic::Type comp,int size)
{
	return videoMixer.CreateMosaic(comp,size);
}

/************************
* CreateSidebar
* 	Add a sidebar to the conference
*************************/
int MultiConf::CreateSidebar()
{
	return audioMixer.CreateSidebar();
}

/************************
* SetMosaicOverlayImage
* 	Set mosaic overlay image
*************************/
int MultiConf::SetMosaicOverlayImage(int mosaicId,const char* filename)
{
	return videoMixer.SetMosaicOverlayImage(mosaicId,filename);
}
/************************
* ResetMosaicOverlayImage
* 	Reset mosaic overlay image
*************************/
int MultiConf::ResetMosaicOverlay(int mosaicId)
{
	return videoMixer.ResetMosaicOverlay(mosaicId);
}
/************************
* DeleteMosaic
* 	delete mosaic
*************************/
int MultiConf::DeleteMosaic(int mosaicId)
{
	return videoMixer.DeleteMosaic(mosaicId);
}

/************************
* DeleteSidebar
* 	delete sidebar
*************************/
int MultiConf::DeleteSidebar(int sidebarId)
{
	return audioMixer.DeleteSidebar(sidebarId);
}

/************************
* CreateParticipant
* 	A�ade un participante
*************************/
int MultiConf::CreateParticipant(int mosaicId,int sidebarId,const std::wstring &name,const std::wstring &token,Participant::Type type)
{
	wchar_t uuid[1024];
	Participant *part = NULL;

	Log(">CreateParticipant [mosaic:%d]\n",mosaicId);

	//SI no tamos iniciados pasamos
	if (!inited)
		return Error("Not inited\n");

	//Get lock
	participantsLock.WaitUnusedAndLock();

	//Obtenemos el id
	int partId = maxId++;

	//Create uuid
	swprintf(uuid,1024,L"%ls@%d",tag.c_str(),partId);

	//Unlock
	participantsLock.Unlock();

	//Le creamos un mixer
	if (!videoMixer.CreateMixer(partId,name))
		return Error("Couldn't set video mixer\n");

	//Y el de audio
	if (!audioMixer.CreateMixer(partId))
	{
		//Borramos el de video
		videoMixer.DeleteMixer(partId);
		//Y salimos
		return Error("Couldn't set audio mixer\n");
	}

	//And text
	if (!textMixer.CreateMixer(partId,name))
	{
		//Borramos el de video y audio
		videoMixer.DeleteMixer(partId);
		audioMixer.DeleteMixer(partId);
		//Y salimos
		return Error("Couldn't set text mixer\n");
	}

	//Depending on the type
	switch(type)
	{
		case Participant::RTP:
			//Create RTP Participant
			part = new RTPParticipant(partId,token,std::wstring(uuid));
			break;
		case Participant::RTMP:
			part = new RTMPParticipant(partId,token);
			//Create RTP Participant
			break;
	}
	
	//Set name
	part->SetName(name);

	//Set inputs and outputs
	part->SetVideoInput(videoMixer.GetInput(partId));
	part->SetVideoOutput(videoMixer.GetOutput(partId));
	part->SetAudioInput(audioMixer.GetInput(partId));
	part->SetAudioOutput(audioMixer.GetOutput(partId));
	part->SetTextInput(textMixer.GetInput(partId));
	part->SetTextOutput(textMixer.GetOutput(partId));

	//Init participant
	part->Init();

	//E iniciamos el mixer
	videoMixer.InitMixer(partId,mosaicId);
	audioMixer.InitMixer(partId,sidebarId);
	textMixer.InitMixer(partId);

	//Get lock
	participantsLock.WaitUnusedAndLock();

	//Lo insertamos en el map
	participants[partId] = part;

	//Unlock
	participantsLock.Unlock();

	//Set us as listener
	part->SetListener(this);

	Log("<CreateParticipant [%d]\n",partId);

	return partId;
}

/************************
* DeleteParticipant
* 	Borra un participante
*************************/
int MultiConf::DeleteParticipant(int id)
{
	Log(">DeleteParticipant [%d]\n",id);

	//Stop recording participant just in case
	StopRecordingParticipant(id);

	//Block
	participantsLock.WaitUnusedAndLock();

	//El iterator
	Participants::iterator it = participants.find(id);

	//Si no esta
	if (it == participants.end())
	{
		//Unlock
		participantsLock.Unlock();
		//Exit
		return Error("Participant not found\n");
	}

	//LO obtenemos
	Participant *part = it->second;

	//Y lo quitamos del mapa
	participants.erase(it);

	//Unlock
	participantsLock.Unlock();

	//Destroy participatn
	DestroyParticipant(id,part);

	Log("<DeleteParticipant [%d]\n",id);

	return 1;
}

void MultiConf:: DestroyParticipant(int partId,Participant* part)
{
	Log(">DestroyParticipant [%d]\n",partId);

	//End participant audio and video streams
	part->End();

	Log("-DestroyParticipant ending mixers [%d]\n",partId);

	//End participant mixers
	videoMixer.EndMixer(partId);
	audioMixer.EndMixer(partId);
	textMixer.EndMixer(partId);

	Log("-DestroyParticipant deleting mixers [%d]\n",partId);

	//QUitamos los mixers
	videoMixer.DeleteMixer(partId);
	audioMixer.DeleteMixer(partId);
	textMixer.DeleteMixer(partId);

	//Delete participant
	delete part;

	Log("<DestroyParticipant [%d]\n",partId);
}

Participant *MultiConf::GetParticipant(int partId)
{
	//Find participant
	Participants::iterator it = participants.find(partId);

	//If not found
	if (it == participants.end())
	{
		//Error
		Error("Participant not found\n");
		//Exit
		return NULL;
	}

	//Get the participant
	return it->second;
}

Participant *MultiConf::GetParticipant(int partId,Participant::Type type)
{
	//Find participant
	Participant *part = GetParticipant(partId);

	//If no participant
	if (!part)
		//Exit
		return NULL;

	//Ensure it is from the correct type
	if (part->GetType()!=type)
	{
		//Error
		Error("Participant is not of desired type\n");
		//Exit
		return NULL;
	}

	//Return it
	return part;
}

/************************
* End
* 	Termina una multiconferencia
*************************/
int MultiConf::End()
{
	Log(">End multiconf\n");

	//End watchers
	StopBroadcaster();

	//End mixers
	audioMixer.EndMixer(watcherId);
	textMixer.EndMixer(watcherId);

	//Stop encoding
	audioEncoder.StopEncoding();
	textEncoder.StopEncoding();

	//End encoders
	audioEncoder.End();
	textEncoder.End();

	//End mixers
	audioMixer.DeleteMixer(watcherId);
	textMixer.DeleteMixer(watcherId);

	//Get lock
	participantsLock.WaitUnusedAndLock();

	//Destroy all participants
	for(Participants::iterator it=participants.begin(); it!=participants.end(); it++)
		//Destroy it
		DestroyParticipant(it->first,it->second);

	//Clear list
	participants.clear();

	//Unlock
	participantsLock.Unlock();

	//Remove all players
	while(players.size()>0)
		//Delete the first one
		DeletePlayer(players.begin()->first);

	//Stop app mixer
	videoMixer.EndMixer(AppMixerId);

	//End it
	appMixer.End();

	//Delete mixer
	videoMixer.DeleteMixer(AppMixerId);

	Log("-End conference mixers\n");

	//Terminamos los mixers
	videoMixer.End();
	audioMixer.End();
	textMixer.End();

	//End chat
	chat.End();
	
	//No inicado
	inited = 0;

	Log("<End multiconf\n");
}

/************************
* SetVideoCodec
* 	SetVideoCodec
*************************/
int MultiConf::SetVideoCodec(int id,int codec,int mode,int fps,int bitrate,int intraPeriod,const Properties &properties)
{
	int ret = 0;

	Log("-SetVideoCodec [%d]\n",id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	Participant *part = GetParticipant(id);

	//Check particpant
	if (part)
		//Set video codec
		ret = part->SetVideoCodec((VideoCodec::Type)codec,mode,fps,bitrate,intraPeriod,properties);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::SetLocalCryptoSDES(int id,MediaFrame::Type media,const char *suite,const char* key)
{
	int ret = 0;

	Log("-SetLocalCryptoSDES %s [partId:%d]\n",MediaFrame::TypeToString(media),id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set  codec
		ret = part->SetLocalCryptoSDES(media,suite,key);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::SetRemoteCryptoSDES(int id,MediaFrame::Type media,const char *suite,const char* key)
{
	int ret = 0;

	Log("-SetRemoteCryptoSDES %s [partId:%d]\n",MediaFrame::TypeToString(media),id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set  codec
		ret = part->SetRemoteCryptoSDES(media,suite,key);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::SetRemoteCryptoDTLS(int id,MediaFrame::Type media,const char *setup,const char *hash,const char *fingerprint)
{
	int ret = 0;

	Log("-SetRemoteCryptoDTLS %s [partId:%d]\n",MediaFrame::TypeToString(media),id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set  codec
		ret = part->SetRemoteCryptoDTLS(media,setup,hash,fingerprint);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::SetLocalSTUNCredentials(int id,MediaFrame::Type media,const char *username,const char* pwd)
{
	int ret = 0;

	Log("-SetLocalSTUNCredentials %s [partId:%d,username:%s,pwd:%s]\n",MediaFrame::TypeToString(media),id,username,pwd);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set  codec
		ret = part->SetLocalSTUNCredentials(media,username,pwd);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}
int MultiConf::SetRTPProperties(int id,MediaFrame::Type media,const Properties& properties)
{
	int ret = 0;

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set  codec
		ret = part->SetRTPProperties(media,properties);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}
int MultiConf::SetRemoteSTUNCredentials(int id,MediaFrame::Type media,const char *username,const char* pwd)
{
	int ret = 0;

	Log("-SetRemoteSTUNCredentials %s [partId:%d,username:%s,pwd:%s]\n",MediaFrame::TypeToString(media),id,username,pwd);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set  codec
		ret = part->SetRemoteSTUNCredentials(media,username,pwd);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::StartSending(int id,MediaFrame::Type media,char *sendIp,int sendPort,const RTPMap& rtpMap,const RTPMap& aptMap)
{
	int ret = 0;

	Log("-StartSending %s [partId:%d]\n",MediaFrame::TypeToString(media),id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set  codec
		ret = part->StartSending(media,sendIp,sendPort,rtpMap,aptMap);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::StopSending(int id,MediaFrame::Type media)
{
	int ret = 0;

	Log("-StopSending %s [partId:%d]\n",MediaFrame::TypeToString(media),id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set video codec
		ret = part->StopSending(media);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::StartReceiving(int id,MediaFrame::Type media,const RTPMap& rtpMap,const RTPMap& aptMap)
{
	int ret = 0;

	Log("-StartReceiving %s [partId:%d]\n",MediaFrame::TypeToString(media),id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set video codec
		ret = part->StartReceiving(media,rtpMap,aptMap);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

/************************
* StopReceivingVideo
* 	StopReceivingVideo
*************************/
int MultiConf::StopReceiving(int id,MediaFrame::Type media)
{
	int ret = 0;

	Log("-StopReceiving %s [partId:%d]\n",MediaFrame::TypeToString(media),id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	RTPParticipant *part = (RTPParticipant*)GetParticipant(id,Participant::RTP);

	//Check particpant
	if (part)
		//Set video codec
		ret = part->StopReceiving(media);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

/************************
* SetAudioCodec
* 	SetAudioCodec
*************************/
int MultiConf::SetAudioCodec(int id,int codec,const Properties& properties)
{
	int ret = 0;

	Log("-SetAudioCodec [%d]\n",id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	Participant *part = GetParticipant(id);

	//Check particpant
	if (part)
		//Set video codec
		ret = part->SetAudioCodec((AudioCodec::Type)codec,properties);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

/************************
* SetTextCodec
* 	SetTextCodec
*************************/
int MultiConf::SetTextCodec(int id,int codec)
{
	int ret = 0;

	Log("-SetTextCodec [%d]\n",id);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	Participant *part = GetParticipant(id);

	//Check particpant
	if (part)
		//Set video codec
		ret = part->SetTextCodec((TextCodec::Type)codec);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

/************************
* SetParticipantMosaic
* 	Change participant mosaic
*************************/
int MultiConf::SetParticipantMosaic(int partId,int mosaicId)
{
	int ret = 0;

	Log("-SetParticipantMosaic [partId:%d,mosaic:%d]\n",partId,mosaicId);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	Participant *part = GetParticipant(partId);

	//If it is the app mixer mosaic
	if (mosaicId==-1)
		//Get it
		mosaicId = appMixerEncoderPrivateMosaicId;
		

	//Check particpant
	if (part)
		//Set it in the video mixer
		ret =  videoMixer.SetMixerMosaic(partId,mosaicId);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}


/************************
* SetParticipantSidebar
* 	Change participant sidebar
*************************/
int MultiConf::SetParticipantSidebar(int partId,int sidebarId)
{
	int ret = 0;

	Log("-SetParticipantSidebar [partId:%d,sidebar:%d]\n",partId,sidebarId);

	//Use list
	participantsLock.IncUse();

	//Get the participant
	Participant *part = GetParticipant(partId);

	//Check particpant
	if (part)
		//Set it in the video mixer
		ret =  audioMixer.SetMixerSidebar(partId,sidebarId);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

/************************
* CreatePlayer
* 	Create a media player
*************************/
int MultiConf::CreatePlayer(int privateId,std::wstring name)
{
	Log(">CreatePlayer [%d]\n",privateId);


	//SI no tamos iniciados pasamos
	if (!inited)
		return Error("Not inited\n");

	//Obtenemos el id
	int playerId = maxId++;

	//Le creamos un mixer
	if (!videoMixer.CreateMixer(playerId,name))
		return Error("Couldn't set video mixer\n");

	//Y el de audio
	if (!audioMixer.CreateMixer(playerId))
	{
		//Borramos el de video
		videoMixer.DeleteMixer(playerId);
		//Y salimos
		return Error("Couldn't set audio mixer\n");
	}

	//If it has private text
	if (privateId)
	{
		//Add a pivate text
		if (!textMixer.CreatePrivate(playerId,privateId,name))
		{
			//Borramos el de video y audio
			videoMixer.DeleteMixer(playerId);
			audioMixer.DeleteMixer(playerId);
			//Y salimos
			return Error("Couldn't set text mixer\n");
		}
	}

	//Create player
	MP4Player *player = new MP4Player();

	//Init
	player->Init(audioMixer.GetOutput(playerId),videoMixer.GetOutput(playerId),textMixer.GetOutput(playerId));

	//E iniciamos el mixer
	videoMixer.InitMixer(playerId,-1);
	audioMixer.InitMixer(playerId,-1);
	textMixer.InitPrivate(playerId);

	//Lo insertamos en el map
	players[playerId] = player;

	Log("<CreatePlayer [%d]\n",playerId);

	return playerId;
}
/************************
* StartPlaying
* 	Start playing the media in the player
*************************/
int MultiConf::StartPlaying(int playerId,const char* filename,bool loop)
{
	Log("-Start playing [id:%d,file:\"%s\",loop:%d]\n",playerId,filename,loop);

	//Find it
	Players::iterator it = players.find(playerId);

	//Si no esta
	if (it == players.end())
		//Not found
		return Error("-Player not found\n");

	//Play
	return it->second->Play(filename,loop);
}
/************************
* StopPlaying
* 	Stop the media playback
*************************/
int MultiConf::StopPlaying(int playerId)
{
	Log("-Stop playing [id:%d]\n",playerId);

	//Find it
	Players::iterator it = players.find(playerId);

	//Si no esta
	if (it == players.end())
		//Not found
		return Error("-Player not found\n");

	//Play
	return it->second->Stop();
}

/************************
* DeletePlayer
* 	Delete a media player
*************************/
int MultiConf::DeletePlayer(int id)
{
	Log(">DeletePlayer [%d]\n",id);


	//El iterator
	Players::iterator it = players.find(id);

	//Si no esta
	if (it == players.end())
		//Not found
		return Error("-Player not found\n");

	//LO obtenemos
	MP4Player *player = (*it).second;

	//Y lo quitamos del mapa
	players.erase(it);

	//Terminamos el audio y el video
	player->Stop();

	Log("-DeletePlayer ending player [%d]\n",id);

	//Paramos el mixer
	videoMixer.EndMixer(id);
	audioMixer.EndMixer(id);
	textMixer.EndPrivate(id);

	//End it
	player->End();

	//QUitamos los mixers
	videoMixer.DeleteMixer(id);
	audioMixer.DeleteMixer(id);
	textMixer.DeletePrivate(id);

	//Lo borramos
	delete player;

	Log("<DeletePlayer [%d]\n",id);

	return 1;
}

int MultiConf::StartRecordingParticipant(int partId,const char* filename)
{
	int ret = 0;

	Log("-StartRecordingParticipant [id:%d,name:\"%s\"]\n",partId,filename);

	//Lock
	participantsLock.IncUse();

	//Get participant
	RTPParticipant *rtp = (RTPParticipant*)GetParticipant(partId,Participant::RTP);

	//Check if
	if (!rtp)
		//End
		goto end;

	//Create recording
	if (!rtp->recorder.Create(filename))
		//End
		goto end;

        //Start recording
        if (!rtp->recorder.Record())
		//End
		goto end;

	//Set the listener for the rtp video packets
	rtp->SetMediaListener(&rtp->recorder);

	//Add the listener for audio and text frames of the watcher
	audioEncoder.AddListener(&rtp->recorder);
	textEncoder.AddListener(&rtp->recorder);

	//OK
	ret = 1;

end:
	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::StopRecordingParticipant(int partId)
{
	int ret = 0;

	Log("-StopRecordingParticipant [id:%d]\n",partId);

	//Lock
	participantsLock.IncUse();

	//Get rtp participant
	RTPParticipant* rtp = (RTPParticipant*)GetParticipant(partId,Participant::RTP);

	//Check participant
	if (rtp)
	{
		//Set the listener for the rtp video packets
		rtp->SetMediaListener(NULL);

		//Add the listener for audio and text frames of the watcher
		audioEncoder.RemoveListener(&rtp->recorder);
		textEncoder.RemoveListener(&rtp->recorder);

		//Stop recording
		rtp->recorder.Stop();

		//End recording
		ret = rtp->recorder.Close();
	}

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

int MultiConf::SendFPU(int partId)
{
	int ret = 0;

	Log("-SendFPU [id:%d]\n",partId);

	//Lock
	participantsLock.IncUse();

	//Get participant
	Participant *part = GetParticipant(partId);

	//Check participant
	if (part)
		//Send FPU
		ret = part->SendVideoFPU();

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

MultiConf::ParticipantStatistics* MultiConf::GetParticipantStatistic(int partId)
{
	//Create statistics map
	ParticipantStatistics *stats = new ParticipantStatistics();

	//Lock
	participantsLock.IncUse();

	//Find participant
	Participant* part = GetParticipant(partId);

	//Check participant
	if (part)
	{
		//Append
		(*stats)["audio"] = part->GetStatistics(MediaFrame::Audio);
		(*stats)["video"] = part->GetStatistics(MediaFrame::Video);
		(*stats)["text"]  = part->GetStatistics(MediaFrame::Text);
	}

	//Unlock
	participantsLock.DecUse();

	//Return stats
	return stats;
}

/********************************************************
 * SetMute
 *   Set participant mute
 ********************************************************/
int MultiConf::SetMute(int partId,MediaFrame::Type media,bool isMuted)
{
	int ret = 0;

	Log("-SetMute [id:%d,media:%d,muted:%d]\n",partId,media,isMuted);

	//Lock
	participantsLock.IncUse();

	//Get participant
	Participant *part = GetParticipant(partId);

	//Check participant
	if (part)
		//Send FPU
		ret = part->SetMute(media,isMuted);

	//Unlock
	participantsLock.DecUse();

	//Exit
	return ret;
}

/********************************************************
 * AddConferenceToken
 *   Add a token for conference watcher
 ********************************************************/
bool MultiConf::AddBroadcastToken(const std::wstring &token)
{
	Log("-AddBroadcastToken [token:\"%ls\"]\n",token.c_str());

	//Check if the pin is already in use
	if (tokens.find(token)!=tokens.end())
		//Broadcast not found
		return Error("Token already in use\n");

	//Add to the pin list
	tokens.insert(token);

	return true;
}
/********************************************************
 * AddParticipantInputToken
 *   Add a token for participant input
 ********************************************************/
bool  MultiConf::AddParticipantInputToken(int partId,const std::wstring &token)
{
	Log("-AddParticipantInputToken [id:%d,token:\"%ls\"]\n",partId,token.c_str());

	//Check if the pin is already in use
	if (tokens.find(token)!=tokens.end())
		//Broadcast not found
		return Error("Token already in use\n");

	//Add to the pin list
	inputTokens[token] = partId;

	return true;
}
/********************************************************
 * AddParticipantOutputToken
 *   Add a token for participant output
 ********************************************************/
bool  MultiConf::AddParticipantOutputToken(int partId,const std::wstring &token)
{
	Log("-AddParticipantOutputToken [id:%d,token:\"%ls\"]\n",partId,token.c_str());

	//Check if the pin is already in use
	if (tokens.find(token)!=tokens.end())
		//Broadcast not found
		return Error("Token already in use\n");

	//Add to the pin list
	outputTokens[token] = partId;

	return true;
}

/********************************************************
 * ConsumeBroadcastToken
 *   Check and consume a token for conference watcher
 ********************************************************/
RTMPMediaStream*  MultiConf::ConsumeBroadcastToken(const std::wstring &token)
{
	//Check token
	BroadcastTokens::iterator it = tokens.find(token);

	//Check we found one
	if (it==tokens.end())
	{
		//Error
		Error("Broadcast token not found\n");
		//Broadcast not found
		return NULL;
	}

	//Remove token
	tokens.erase(it);

	//It is valid so return encoder
	return &flvEncoder;
}

RTMPMediaStream::Listener* MultiConf::ConsumeParticipantInputToken(const std::wstring &token)
{
	//Check token
	ParticipantTokens::iterator it = inputTokens.find(token);

	//Check we found one
	if (it==inputTokens.end())
	{
		//Error
		Error("Participant token not found\n");
		//Broadcast not found
		return NULL;
	}

	//Get participant id
	int partId = it->second;

	//Remove token
	inputTokens.erase(it);

	//Get it
	Participants::iterator itPart = participants.find(partId);

	//Check if not found
	if (itPart==participants.end())
	{
		//Error
		Error("Participant not found\n");
		//Broadcast not found
		return NULL;
	}

	//Get it
	Participant* part = itPart->second;

	//Asert correct tipe
	if (part->GetType()!=Participant::RTMP)
	{
		//Error
		Error("Participant type not RTMP");

		//Broadcast not found
		return NULL;
	}

	//return it
	return (RTMPMediaStream::Listener*)(RTMPParticipant*)part;
}

RTMPMediaStream* MultiConf::ConsumeParticipantOutputToken(const std::wstring &token)
{
	//Check token
	ParticipantTokens::iterator it = outputTokens.find(token);

	//Check we found one
	if (it==outputTokens.end())
	{
		//Error
		Error("Participant token not found\n");
		//Broadcast not found
		return NULL;
	}

	//Get participant id
	int partId = it->second;

	//Remove token
	outputTokens.erase(it);

	//Get it
	Participants::iterator itPart = participants.find(partId);

	//Check if not found
	if (itPart==participants.end())
	{
		//Error
		Error("Participant not found\n");
		//Broadcast not found
		return NULL;
	}

	//Get it
	Participant* part = itPart->second;

	//Asert correct tipe
	if (part->GetType()!=Participant::RTMP)
	{
		//Error
		Error("Participant not RTMP type\n");
		//Broadcast not found
		return NULL;
	}

	//return it
	return (RTMPMediaStream*)(RTMPParticipant*)part;
}

/********************************
 * NetConnection
 **********************************/
RTMPNetStream::shared MultiConf::CreateStream(DWORD streamId,DWORD audioCaps,DWORD videoCaps,RTMPNetStream::Listener *listener)
{
	//No stream for that url
	auto stream = std::make_shared<NetStream>(streamId,this,listener);

	//Set tag
	stream->SetTag(tag);

	//Register the sream
	RegisterStream(stream);

	//Create stream
	return stream;
}

void MultiConf::DeleteStream(const RTMPNetStream::shared& stream)
{
	//Unregister stream
	UnRegisterStream(stream);
}

/*****************************************************
 * RTMP Broadcast session
 *
 ******************************************************/
MultiConf::NetStream::NetStream(DWORD streamId,MultiConf *conf,RTMPNetStream::Listener* listener) : RTMPNetStream(streamId,listener)
{
	//Store conf
	this->conf = conf;
	//Not opened
	opened = false;
}

MultiConf::NetStream::~NetStream()
{
	//Close
	Close();
	//Not opened
	opened = false;
}

/***************************************
 * Play
 *	RTMP event listener
 **************************************/
void MultiConf::NetStream::doPlay(QWORD transId, std::wstring& url,RTMPMediaStream::Listener* listener)
{
	RTMPMediaStream *stream = NULL;

	//Log
	Log("-Play stream [%d,%ls]\n",GetStreamId(),url.c_str());

	//Check  if already in use
	if (opened)
	{
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Stream already in use");
		//Noting found
		Error("Stream #%d already in use\n", GetStreamId());
		//Exit
		return;
	}

	//Skip the type part "flv:" inserted by FMS
	size_t i = url.find(L":",0);

	//If not found
	if (i==std::wstring::npos)
		//from the begining
		i = 0;
	else
		//Skip
		i++;

	//Get the next "/"
	size_t j = url.find(L"/",i);

	//Check if found
	if (j==std::wstring::npos)
	{
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Play::Failed,L"Wrong format stream name");
		//Noting found
		Error("Wrong format stream name\n");
		//Exit
		return;
	}
	//Get type
	std::wstring type = url.substr(i,j-i);

	//Get token
	std::wstring token = url.substr(j+1,url.length()-j);

	//Check app name
	if (type.compare(L"participant")==0)
	{
		//Get participant stream
		stream = conf->ConsumeParticipantOutputToken(token);
		//Wait for intra
		SetWaitIntra(true);
		//And rewrite
		SetRewriteTimestamps(true);
	} else if (type.compare(L"watcher")==0) {
		//Get broadcast stream
		stream = conf->ConsumeBroadcastToken(token);
		//Wait for intra
		SetWaitIntra(true);
		//And rewrite
		SetRewriteTimestamps(true);
	} else {
		//Log
		Error("-Application type name incorrect [%ls]\n",type.c_str());
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Play::Failed,L"Type invalid");
		//Nothing done
		return;
	}

	//Check if found
	if (!stream)
	{
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Play::StreamNotFound,L"Token invalid");
		//Exit
		return;
	}

	//Opened
	opened = true;

	//Send reseted status
	fireOnNetStreamStatus(transId, RTMP::Netstream::Play::Reset,L"Playback reset");
	//Send play status
	fireOnNetStreamStatus(transId, RTMP::Netstream::Play::Start,L"Playback started");

	//Add listener
	AddMediaListener(listener);
	//Attach
	Attach(stream);
}

void MultiConf::NetStream::doSeek(QWORD transId, DWORD time)
{
	//Send status
	fireOnNetStreamStatus(transId, RTMP::Netstream::Seek::Failed,L"Seek not supported");
}
/***************************************
 * Publish
 *	RTMP event listener
 **************************************/
void MultiConf::NetStream::doPublish(QWORD transId, std::wstring& url)
{
	//Log
	Log("-Publish stream [%ls]\n",url.c_str());

	//Check  if already in use
	if (opened)
	{
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Failed,L"Stream already in use");
		//Noting found
		Error("Stream #%d already in use\n", GetStreamId());
		//Exit
		return;
	}

	//Skip the type part "flv:" inserted by FMS
	size_t i = url.find(L":",0);

	//If not found
	if (i==std::wstring::npos)
		//from the begining
		i = 0;
	else
		//Skip
		i++;

	//Get the next "/"
	size_t j = url.find(L"/",i);

	//Check if found
	if (j==std::wstring::npos)
	{
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Publish::BadName,L"Wrong format stream name");
		//Noting found
		Error("Wrong format stream name\n");
		//Exit
		return;
	}
	//Get type
	std::wstring type = url.substr(i,j-i);

	//Get token
	std::wstring token = url.substr(j+1,url.length()-j);

	//Check app name
	if (!type.compare(L"participant")==0)
	{
		//Log
		Error("-Application type name incorrect [%ls]\n",type.c_str());
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Publish::BadName,L"Type invalid");
		//Nothing done
		return;
	}

	//Get participant stream
	RTMPMediaStream::Listener *listener = conf->ConsumeParticipantInputToken(token);

	//Check if found
	if (!listener)
	{
		//Send error
		fireOnNetStreamStatus(transId, RTMP::Netstream::Publish::BadName,L"Token invalid");
		//Exit
		return;
	}

	//Opened
	opened = true;

	//Add this as listener
	AddMediaListener(listener);

	//Send publish notification
	fireOnNetStreamStatus(transId, RTMP::Netstream::Publish::Start,L"Publish started");
}

void MultiConf::NetStream::doClose(QWORD transId, RTMPMediaStream::Listener *listener)
{
	//Close
	Close();
}

void MultiConf::NetStream::Close()
{
	Log(">Close multiconf netstream\n");

	///Remove listener just in case
	RemoveAllMediaListeners();
	//Dettach if playing
	Detach();

	Log("<Closed\n");
}

void MultiConf::onRequestFPU(Participant *part)
{
	//Check listener
	if (listener)
		//Send event
		listener->onParticipantRequestFPU(this,part->GetPartId(),this->param);
}

int MultiConf::AppMixerDisplayImage(const char* filename)
{
	//Display it
	return appMixer.DisplayImage(filename);
}



int MultiConf::WebsocketConnectRequest(WebSocket *ws,int partId,const std::string &token,const std::string &to)
{
	Participant* part = NULL;
	
	//Convert token to wstring
	UTF8Parser utf8(token);

	//Get participant lock
	participantsLock.IncUse();

	//Get participant
	Participants::iterator it = participants.find(partId);

	//Check if found
	if (it==participants.end())
		//Exit
		goto forbiden;
	
	//Get participant
	part = it->second;

	//compare token
	if (utf8.GetWString().compare(it->second->GetToken())!=0)
		//Exit
		goto forbiden;

	//Check what are they connecting to
	if (to.compare("chat")==0)
		//Connect to chat
		chat.WebsocketConnectRequest(partId,ws);
	else
		//Exit
		goto forbiden;

	//Dec use
	participantsLock.DecUse();

	//OK
	return 1;

forbiden:
	//Reject it
	ws->Reject(403,"Forbidden");

	//Dec use
	participantsLock.DecUse();

	//Error
	return 0;
}


int  MultiConf::StartPublishing(const char* server,int port, const char* app,const char* stream)
{

	PublisherInfo info;
	UTF8Parser parser;

	//Parse stream name
	parser.SetString(stream);

	//LOg
	Log(">StartPublishing broadcast to [url=\"rtmp://%s:%d/%s\",stream=\"%ls\"\n",server,port,app,parser.GetWChar());

	//Pa porsi
	if (!inited)
		//Exit
		return Error("Multiconf not initied");

	//Get published id
	info.id = maxPublisherId++;

	//Store published stream name
	info.name = parser.GetWString();

	//Create new publisherf1722
	info.conn = new RTMPClientConnection(tag);

	//Store id as user data
	info.conn->SetUserData(info.id);

	//No stream
	info.stream = NULL;

	//Add to map
	publishers[info.id] = info;

	//Connect
	info.conn->Connect(server,port,app,this);

	Log("<StartPublishing broadcast [id%d]\n",info.id);

	//Return id
	return info.id;
}

int  MultiConf::StopPublishing(int id)
{
	Log("-StopPublishing broadcast [id:%d]\n",id);

	//Find it
	Publishers::iterator it = publishers.find(id);

	//If not found
	if (it==publishers.end())
		//Exit
		return Error("-Publisher not found\n");

	//Get info
	PublisherInfo info = it->second;

	//Check it has an stream opened
	if (info.stream)
	{
		//Un publish
		info.stream->UnPublish();
		//And close
		info.stream->Close();
	}
	//Disconnect
	info.conn->Disconnect();
	//Exit
	return 1;
}

void MultiConf::onConnected(RTMPClientConnection* conn)
{
	//Get id
	DWORD id = conn->GetUserData();
	//Log
	Log("-RTMPClientConnection connected [id:%d]\n",id);
	//Find it
	Publishers::iterator it = publishers.find(id);
	//If found
	if (it!=publishers.end())
	{
		//Get publisher info
		PublisherInfo& info = it->second;
		//Release stream
		conn->Call(L"releaseStream",new AMFNull,new AMFString(info.name));
		//Publish
		conn->Call(L"FCPublish",new AMFNull,new AMFString(info.name));
		//Create stream
		conn->CreateStream(tag);
	} else {
		Log("-RTMPClientConnection connection not found\n");
	}
}

void MultiConf::onNetStreamCreated(RTMPClientConnection* conn,RTMPClientConnection::NetStream *stream)
{
	//Get id
	DWORD id = conn->GetUserData();
	//Log
	Log("-RTMPClientConnection onNetStreamCreated [id:%d]\n",id);
	//Find it
	Publishers::iterator it = publishers.find(id);
	//If found
	if (it!=publishers.end())
	{
		//Store sream
		PublisherInfo& info = it->second;
		//Store stream
		info.stream = stream;
		//Do publish url
		stream->Publish(info.name);
		//Wait for intra
		stream->SetWaitIntra(true);
		//Add listener (TODO: move downwards)
		flvEncoder.AddMediaListener(stream);
	}
}

void MultiConf::onCommandResponse(RTMPClientConnection* conn,DWORD id,bool isError,AMFData* param)
{
	//We sould do the add listener here
}
void MultiConf::onDisconnected(RTMPClientConnection* conn)
{
	//TODO: should we lock? I expect so
	//Check if it were ended
	if (inited)
		//Do nothing it will be handled outside
		return;
	//Get id
	DWORD id = conn->GetUserData();
	//Log
	Log("-RTMPClientConnection onDisconnected [id:%d]\n",id);
	//Find it
	Publishers::iterator it = publishers.find(id);
	//If found
	if (it!=publishers.end())
	{
		//Store sream
		PublisherInfo& info = it->second;
		//If it was an stream
		if (info.stream)
		{
			//Remove listener
			flvEncoder.RemoveMediaListener((RTMPMediaStream::Listener*)info.stream);
			//Delete it
			delete(info.stream);
		}
		//Delete connection
		delete(info.conn);
		//Remove
		publishers.erase(it);
	}
}
