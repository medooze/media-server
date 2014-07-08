#include "log.h"
#include "MediaSession.h"

MediaSession::MediaSession(std::wstring tag)
{
	//Init
	maxEndpointId = 1;
	maxPlayersId = 1;
	maxRecordersId = 1;
	maxAudioMixerId = 1;
	maxVideoMixerId = 1;
	maxVideoTranscoderId = 1;
	//Store it
	this->tag = tag;
}

MediaSession::~MediaSession()
{
	End();
}

void MediaSession::SetListener(MediaSession::Listener *listener,void* param)
{
	//Store values
	this->listener = listener;
	this->param = param;
}

int MediaSession::Init()
{
	Log("-Init media session\n");
	
	//Inited
	return 1;
}

int MediaSession::End()
{
	Log(">End media session\n");

	//Delete all recorders
	for (Recorders::iterator it=recorders.begin(); it!=recorders.end(); ++it)
		//Delete object
		delete(it->second);
	//Clean map
	recorders.clear();

	//End all endpoints
	for (Endpoints::iterator it=endpoints.begin(); it!=endpoints.end(); ++it)
		//End it
		it->second->End();

	//Delete all players
	for (Players::iterator it=players.begin(); it!=players.end(); ++it)
		//Delete object
		delete(it->second);
	//Clean map
	players.clear();

	//Delete all video transcoders
	for (VideoTranscoders::iterator it=videoTranscoders.begin(); it!=videoTranscoders.end(); ++it)
		//Delete object
		delete(it->second);

	//Clean map
	videoTranscoders.clear();

	//Delete all endpoints
	for (Endpoints::iterator it=endpoints.begin(); it!=endpoints.end(); ++it)
		//Delete object
		delete(it->second);

	//Clean map
	endpoints.clear();

	Log("<End media session\n");

	return 1;
}


int MediaSession::PlayerCreate(std::wstring tag)
{
        //Create ID
        int playerId = maxPlayersId++;
	//Create player
	Player* player = new Player(tag);
	//Set event listener
	player->SetListener(this,(void*)playerId);
        //Append the player
        players[playerId] = player;
        //Return it
        return playerId;
}

int MediaSession::PlayerOpen(int playerId,const char* filename)
{
        //Get Player
        Players::iterator it = players.find(playerId);

        //If not found
        if (it==players.end())
                //Exit
                return Error("Player not found\n");
        //Get it
        Player* player = it->second;

        return player->Open(filename);
}

int MediaSession::PlayerPlay(int playerId)
{
        //Get Player
        Players::iterator it = players.find(playerId);

        //If not found
        if (it==players.end())
                //Exit
                return Error("Player not found\n");
        //Get it
        Player* player = it->second;

        return player->Play();
}

int MediaSession::PlayerSeek(int playerId,QWORD time)
{
        //Get Player
        Players::iterator it = players.find(playerId);

        //If not found
        if (it==players.end())
                //Exit
                return Error("Player not found\n");
        //Get it
        Player* player = it->second;

        return player->Seek(time);
}

int MediaSession::PlayerStop(int playerId)
{
        //Get Player
        Players::iterator it = players.find(playerId);

        //If not found
        if (it==players.end())
                //Exit
                return Error("Player not found\n");
        //Get it
        Player* player = it->second;

        return player->Stop();
}

int MediaSession::PlayerClose(int playerId)
{
        //Get Player
        Players::iterator it = players.find(playerId);

        //If not found
        if (it==players.end())
                //Exit
                return Error("Player not found\n");
        //Get it
        Player* player = it->second;

        return player->Close();
}

int MediaSession::PlayerDelete(int playerId)
{
        //Get Player
        Players::iterator it = players.find(playerId);

        //If not found
        if (it==players.end())
                //Exit
                return Error("Player not found\n");
        //Get it
        Player* player = it->second;

        //Remove from list
        players.erase(it);

        //Relete player
        delete(player);

        return 1;
}

int MediaSession::RecorderCreate(std::wstring tag)
{
        //Create ID
        int recorderId = maxRecordersId++;
	//Create recorder
	Recorder* recorder = new Recorder(tag);
	//Append the recorder
        recorders[recorderId] = recorder;
        //Return it
        return recorderId;
}

int MediaSession::RecorderRecord(int recorderId,const char* filename)
{
        //Get recorder
        Recorders::iterator it = recorders.find(recorderId);
        //If not found
        if (it==recorders.end())
                //Exit
                return Error("Recorder not found\n");
        //Get it
        Recorder* recorder = it->second;
	//create recording
        if (!recorder->Create(filename))
		//Error
		return Error("-Could not create file");
	//Start recording
	return recorder->Record();
}

int MediaSession::RecorderStop(int recorderId)
{
	//Get recorder
        Recorders::iterator it = recorders.find(recorderId);
        //If not found
        if (it==recorders.end())
                //Exit
                return Error("Recorder not found\n");
        //Get it
        Recorder* recorder = it->second;
	//Stop recording
        return recorder->Close();
}

int MediaSession::RecorderDelete(int recorderId)
{
        //Get Player
        Recorders::iterator it = recorders.find(recorderId);

        //If not found
        if (it==recorders.end())
                //Exit
                return Error("Recorder not found\n");
        //Get it
        Recorder* recorder = it->second;

        //Remove from list
        recorders.erase(it);

        //Relete player
        delete(recorder);

        return 1;
}

int MediaSession::RecorderAttachToAudioMixerPort(int recorderId,int mixerId,int portId)
{
	//Get Player
        Recorders::iterator it = recorders.find(recorderId);

        //If not found
        if (it==recorders.end())
                //Exit
                return Error("Recorder not found\n");
        //Get it
        Recorder* recorder = it->second;

	 //Get Player
        AudioMixers::iterator itMixer = audioMixers.find(mixerId);

        //If not found
        if (itMixer==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");

	 //Get it
        AudioMixerResource* audioMixer = itMixer->second;

	//Attach
	return recorder->Attach(MediaFrame::Audio,audioMixer->GetJoinable(portId));
}

int MediaSession::RecorderAttachToVideoMixerPort(int recorderId,int mixerId,int portId)
{
	//Get Player
        Recorders::iterator it = recorders.find(recorderId);

        //If not found
        if (it==recorders.end())
                //Exit
                return Error("Recorder not found\n");
        //Get it
        Recorder* recorder = it->second;

	 //Get Player
        VideoMixers::iterator itMixer = videoMixers.find(mixerId);

        //If not found
        if (itMixer==videoMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");

	 //Get it
        VideoMixerResource* videoMixer = itMixer->second;

	//And attach
	return recorder->Attach(MediaFrame::Video,videoMixer->GetJoinable(portId));
}

int MediaSession::RecorderAttachToEndpoint(int recorderId,int endpointId,MediaFrame::Type media)
{
	//Get Player
        Recorders::iterator it = recorders.find(recorderId);

        //If not found
        if (it==recorders.end())
                //Exit
                return Error("Recorder not found\n");
        //Get it
        Recorder* recorder = it->second;

	//Get source endpoint
        Endpoints::iterator itEndpoints = endpoints.find(endpointId);

        //If not found
        if (itEndpoints==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* source = itEndpoints->second;

	//Attach
	return recorder->Attach(media,source->GetJoinable(media));
}

int MediaSession::RecorderDettach(int recorderId,MediaFrame::Type media)
{
	//Get Player
        Recorders::iterator it = recorders.find(recorderId);

        //If not found
        if (it==recorders.end())
                //Exit
                return Error("Recorder not found\n");
        //Get it
        Recorder* recorder = it->second;

	//Attach
	return recorder->Dettach(media);
}

int MediaSession::EndpointCreate(std::wstring name,bool audioSupported,bool videoSupported,bool textSupport)
{
        //Create endpoint
        Endpoint* endpoint = new Endpoint(name,audioSupported,videoSupported,textSupport);
	//Init it
	endpoint->Init();
	//Create ID
        int endpointId = maxEndpointId++;
	//Log endpoint tag name
	Log("-EndpointCreate [%d,%ls]\n",endpointId,endpoint->GetName().c_str());
	//Append
	endpoints[endpointId] = endpoint;
        //Return it
        return endpointId;

}
int MediaSession::EndpointDelete(int endpointId)
{
        //Get Player
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointDelete [%ls]\n",endpoint->GetName().c_str());

        //Remove from list
        endpoints.erase(it);

	//End it
	endpoint->End();

        //Relete endpoint
        delete(endpoint);

        return 1;
}

int MediaSession::EndpointSetLocalCryptoSDES(int endpointId,MediaFrame::Type media,const char *suite,const char* key)
{
        //Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Call it
	return endpoint->SetLocalCryptoSDES(media,suite,key);
}

int MediaSession::EndpointSetRemoteCryptoSDES(int endpointId,MediaFrame::Type media,const char *suite,const char* key)
{
        //Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Call it
	return endpoint->SetRemoteCryptoSDES(media,suite,key);
}

int MediaSession::EndpointSetRemoteCryptoDTLS(int endpointId,MediaFrame::Type media,const char *setup,const char *hash,const char *fingerprint)
{
        //Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Call it
	return endpoint->SetRemoteCryptoDTLS(media,setup,hash,fingerprint);
}


int MediaSession::EndpointSetLocalSTUNCredentials(int endpointId,MediaFrame::Type media,const char *username,const char* pwd)
{
        //Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Call it
	return endpoint->SetLocalSTUNCredentials(media,username,pwd);
}

int MediaSession::EndpointSetRemoteSTUNCredentials(int endpointId,MediaFrame::Type media,const char *username,const char* pwd)
{
        //Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Call it
	return endpoint->SetRemoteSTUNCredentials(media,username,pwd);
}

int MediaSession::EndpointSetRTPProperties(int endpointId,MediaFrame::Type media,const Properties& properties)
{
        //Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;
	//Call it
	return endpoint->SetRTPProperties(media,properties);
}

//Endpoint Video functionality
int MediaSession::EndpointStartSending(int endpointId,MediaFrame::Type media,char *sendVideoIp,int sendVideoPort,RTPMap& rtpMap)
{
        //Get Player
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointStartSending [%ls]\n",endpoint->GetName().c_str());

	//Execute
	return endpoint->StartSending(media,sendVideoIp, sendVideoPort, rtpMap);
}

int MediaSession::EndpointStopSending(int endpointId,MediaFrame::Type media)
{
        //Get Player
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointStopSending [%ls]\n",endpoint->GetName().c_str());

	//Execute
	return endpoint->StopSending(media);
}

int MediaSession::EndpointStartReceiving(int endpointId,MediaFrame::Type media,RTPMap& rtpMap)
{
        //Get Player
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointStartReceiving [%ls]\n",endpoint->GetName().c_str());

	//Execute
	return endpoint->StartReceiving(media,rtpMap);
}
int MediaSession::EndpointStopReceiving(int endpointId,MediaFrame::Type media)
{
        //Get Player
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointStartReceiving [%ls]\n",endpoint->GetName().c_str());
	
	//Execute
	return endpoint->StopReceiving(media);
}

int MediaSession::EndpointRequestUpdate(int endpointId,MediaFrame::Type media)
{
	//Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointRequestUpdate [%ls]\n",endpoint->GetName().c_str());

	//Execute
	return endpoint->RequestUpdate(media);
}
int MediaSession::EndpointAttachToPlayer(int endpointId,int playerId,MediaFrame::Type media)
{
	//Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointAttachToPlayer [%ls]\n",endpoint->GetName().c_str());

	 //Get Player
        Players::iterator itPlayer = players.find(playerId);

        //If not found
        if (itPlayer==players.end())
                //Exit
                return Error("Player not found\n");
	
	 //Get it
        Player* player = itPlayer->second;
	
	//Attach
	return endpoint->Attach(media,player->GetJoinable(media));
}

int MediaSession::EndpointAttachToAudioMixerPort(int endpointId,int mixerId,int portId)
{
	//Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointAttachToAudioMixerPort [%ls]\n",endpoint->GetName().c_str());

	 //Get Player
        AudioMixers::iterator itMixer = audioMixers.find(mixerId);

        //If not found
        if (itMixer==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");

	 //Get it
        AudioMixerResource* audioMixer = itMixer->second;

	//Attach
	return endpoint->Attach(MediaFrame::Audio,audioMixer->GetJoinable(portId));
}

int MediaSession::EndpointAttachToVideoMixerPort(int endpointId,int mixerId,int portId)
{
	//Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointAttachToVideoMixerPort [%ls]\n",endpoint->GetName().c_str());

	 //Get Player
        VideoMixers::iterator itMixer = videoMixers.find(mixerId);

        //If not found
        if (itMixer==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found\n");

	 //Get it
        VideoMixerResource* videoMixer = itMixer->second;

	//And attach
	return endpoint->Attach(MediaFrame::Video,videoMixer->GetJoinable(portId));
}

int MediaSession::EndpointAttachToVideoTranscoder(int endpointId,int videoTranscoderId)
{
	//Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found [%d]\n",endpointId);
        //Get it
        Endpoint* endpoint = it->second;

	 //Get Video transcoder
        VideoTranscoders::iterator itTranscoder = videoTranscoders.find(videoTranscoderId);

        //If not found
        if (itTranscoder==videoTranscoders.end())
                //Exit
                return Error("VideoTranscoder not found[%d]\n",videoTranscoderId);

	 //Get it
        VideoTranscoder* videoTranscoder = itTranscoder->second;

	//Log endpoint tag name
	Log("-EndpointAttachToVideoTranscoder [endpoint:%ls,transcoder:%ls]\n",endpoint->GetName().c_str(),videoTranscoder->GetName().c_str());

	//And attach
	return endpoint->Attach(MediaFrame::Video,videoTranscoder);
}

int MediaSession::EndpointAttachToEndpoint(int endpointId,int sourceId,MediaFrame::Type media)
{
	//Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointAttachToEndpoint [%ls]\n",endpoint->GetName().c_str());

	//Get source endpoint
        it = endpoints.find(sourceId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");
        //Get it
        Endpoint* source = it->second;

	//Attach
	return endpoint->Attach(media,source->GetJoinable(media));
}

int MediaSession::EndpointDettach(int endpointId,MediaFrame::Type media)
{
	//Get endpoint
        Endpoints::iterator it = endpoints.find(endpointId);

        //If not found
        if (it==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");

	//Get it
        Endpoint* endpoint = it->second;

	//Log endpoint tag name
	Log("-EndpointDettach [%ls]\n",endpoint->GetName().c_str());

	//Attach
	return endpoint->Dettach(media);
}

void MediaSession::onEndOfFile(Player *player,void* playerId)
{
	//Check for listener
	if (listener)
		//Send
		listener->onPlayerEndOfFile(this,player,(intptr_t)playerId,param);
}


int MediaSession::AudioMixerCreate(std::wstring tag)
{
        //Create ID
        int audioMixerId = maxAudioMixerId++;
	//Create player
	AudioMixerResource* audioMixer = new AudioMixerResource(tag);
	//Init it
	audioMixer->Init();
        //Append the player
        audioMixers[audioMixerId] = audioMixer;
        //Return it
        return audioMixerId;
}

int MediaSession::AudioMixerDelete(int mixerId)
{
        //Get Player
        AudioMixers::iterator it = audioMixers.find(mixerId);

        //If not found
        if (it==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");
        //Get it
        AudioMixerResource* audioMixer = it->second;

        //Remove from list
        audioMixers.erase(it);

	//End it
	audioMixer->End();

        //Relete audioMixer
        delete(audioMixer);

        return 1;
}

int MediaSession::AudioMixerPortCreate(int mixerId,std::wstring tag)
{
	//Get Player
        AudioMixers::iterator it = audioMixers.find(mixerId);

        //If not found
        if (it==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");
        //Get it
        AudioMixerResource* audioMixer = it->second;

	//Execute
	return audioMixer->CreatePort(tag);
}

int MediaSession::AudioMixerPortSetCodec(int mixerId,int portId,AudioCodec::Type codec)
{
	//Get Player
        AudioMixers::iterator it = audioMixers.find(mixerId);

        //If not found
        if (it==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");
        //Get it
        AudioMixerResource* audioMixer = it->second;

	//Execute
	return audioMixer->SetPortCodec(portId,codec);
}

int MediaSession::AudioMixerPortDelete(int mixerId,int portId)
{
	//Get Player
        AudioMixers::iterator it = audioMixers.find(mixerId);

        //If not found
        if (it==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");
        //Get it
        AudioMixerResource* audioMixer = it->second;

	//Execute
	return audioMixer->DeletePort(portId);
}


int MediaSession::AudioMixerPortAttachToEndpoint(int mixerId,int portId,int endpointId)
{
	//Get mixer
        AudioMixers::iterator it = audioMixers.find(mixerId);

        //If not found
        if (it==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");
        //Get it
        AudioMixerResource* audioMixer = it->second;

	//Get endpoint
        Endpoints::iterator itEnd = endpoints.find(endpointId);

        //If not found
        if (itEnd==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");

        //Get it
        Endpoint* endpoint = itEnd->second;

	//Log endpoint tag name
	Log("-AudioMixerPortAttachToEndpoint [%ls]\n",endpoint->GetName().c_str());

	//Attach
	return audioMixer->Attach(portId,endpoint->GetJoinable(MediaFrame::Audio));
}

int MediaSession::AudioMixerPortAttachToPlayer(int mixerId,int portId,int playerId)
{
	//Get mixer
        AudioMixers::iterator it = audioMixers.find(mixerId);

        //If not found
        if (it==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");
        //Get it
        AudioMixerResource* audioMixer = it->second;

	 //Get Player
        Players::iterator itPlayer = players.find(playerId);

        //If not found
        if (itPlayer==players.end())
                //Exit
                return Error("Player not found\n");

	 //Get it
        Player* player = itPlayer->second;

	//Attach
	return audioMixer->Attach(portId,player->GetJoinable(MediaFrame::Audio));
}

int MediaSession::AudioMixerPortDettach(int mixerId,int portId)
{
	//Get mixer
        AudioMixers::iterator it = audioMixers.find(mixerId);

        //If not found
        if (it==audioMixers.end())
                //Exit
                return Error("AudioMixerResource not found\n");
        //Get it
        AudioMixerResource* audioMixer = it->second;
	
       //Attach
	return audioMixer->Dettach(portId);
}

int MediaSession::VideoMixerCreate(std::wstring tag)
{
        //Create ID
        int videoMixerId = maxVideoMixerId++;
	//Create player
	VideoMixerResource* videoMixer = new VideoMixerResource(tag);
	//Init
	videoMixer->Init(Mosaic::mosaic2x2,PAL);
        //Append the player
        videoMixers[videoMixerId] = videoMixer;
        //Return it
        return videoMixerId;
}

int MediaSession::VideoMixerDelete(int mixerId)
{
        //Get Player
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

        //Remove from list
        videoMixers.erase(it);

	//End it
	videoMixer->End();

        //Relete videoMixer
        delete(videoMixer);

        return 1;
}

int MediaSession::VideoMixerPortCreate(int mixerId,std::wstring tag, int mosaicId)
{
	//Get Player
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

	//Execute
	return videoMixer->CreatePort(tag,mosaicId);
}

int MediaSession::VideoMixerPortSetCodec(int mixerId,int portId,VideoCodec::Type codec,int size,int fps,int bitrate,int intraPeriod,const Properties& properties)
{
	//Get Player
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

	//Execute
	return videoMixer->SetPortCodec(portId,codec,size,fps,bitrate,intraPeriod,properties);
}

int MediaSession::VideoMixerPortDelete(int mixerId,int portId)
{
	//Get Player
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

	//Execute
	return videoMixer->DeletePort(portId);
}


int MediaSession::VideoMixerPortAttachToEndpoint(int mixerId,int portId,int endpointId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

	//Get endpoint
        Endpoints::iterator itEnd = endpoints.find(endpointId);

        //If not found
        if (itEnd==endpoints.end())
                //Exit
                return Error("Endpoint not found\n");

        //Get it
        Endpoint* endpoint = itEnd->second;

	//Log endpoint tag name
	Log("-VideoMixerPortAttachToEndpoint [%ls]\n",endpoint->GetName().c_str());

	//Attach
	return videoMixer->Attach(portId,endpoint->GetJoinable(MediaFrame::Video));
}

int MediaSession::VideoMixerPortAttachToPlayer(int mixerId,int portId,int playerId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

	 //Get Player
        Players::iterator itPlayer = players.find(playerId);

        //If not found
        if (itPlayer==players.end())
                //Exit
                return Error("Player not found\n");

	 //Get it
        Player* player = itPlayer->second;

	//Attach
	return videoMixer->Attach(portId,player->GetJoinable(MediaFrame::Video));
}

int MediaSession::VideoMixerPortDettach(int mixerId,int portId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->Dettach(portId);
}

int MediaSession::VideoMixerMosaicCreate(int mixerId,Mosaic::Type comp,int size)
{
		//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->CreateMosaic(comp,size);
}

int MediaSession::VideoMixerMosaicDelete(int mixerId,int mosaicId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->DeleteMosaic(mosaicId);
}

int MediaSession::VideoMixerMosaicSetSlot(int mixerId,int mosaicId,int num,int portId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->SetSlot(mosaicId,num,portId);
}

int MediaSession::VideoMixerMosaicSetCompositionType(int mixerId,int mosaicId,Mosaic::Type comp,int size)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->SetCompositionType(mosaicId,comp,size);
}

int MediaSession::VideoMixerMosaicSetOverlayPNG(int mixerId,int mosaicId,const char* overlay)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->SetOverlayPNG(mosaicId,overlay);
}

int MediaSession::VideoMixerMosaicResetSetOverlay(int mixerId,int mosaicId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->ResetOverlay(mosaicId);
}

int MediaSession::VideoMixerMosaicAddPort(int mixerId,int mosaicId,int portId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->AddMosaicParticipant(mosaicId,portId);
}

int MediaSession::VideoMixerMosaicRemovePort(int mixerId,int mosaicId,int portId)
{
	//Get mixer
        VideoMixers::iterator it = videoMixers.find(mixerId);

        //If not found
        if (it==videoMixers.end())
                //Exit
                return Error("VideoMixerResource not found [%d]\n",mixerId);
        //Get it
        VideoMixerResource* videoMixer = it->second;

       //Attach
	return videoMixer->RemoveMosaicParticipant(mosaicId,portId);
}

int MediaSession::VideoTranscoderCreate(std::wstring tag)
{
	//Create ID
        int videoTranscoderId = maxVideoTranscoderId++;
	//Create trascoder
	VideoTranscoder* videoTranscoder = new VideoTranscoder(tag);
	//Init
	videoTranscoder->Init();
        //Append the player
        videoTranscoders[videoTranscoderId] = videoTranscoder;
        //Return it
        return videoTranscoderId;
}

int MediaSession::VideoTranscoderFPU(int videoTranscoderId)
{
	//Get Player
        VideoTranscoders::iterator it = videoTranscoders.find(videoTranscoderId);

        //If not found
        if (it==videoTranscoders.end())
                //Exit
                return Error("VideoTranscoder not found [%d]\n",videoTranscoderId);
        //Get it
        VideoTranscoder* videoTranscoder = it->second;

	//Execute
	videoTranscoder->Update();

	//OK
	return 1;
}

int MediaSession::VideoTranscoderSetCodec(int videoTranscoderId,VideoCodec::Type codec,int size,int fps,int bitrate,int intraPeriod,Properties & props)
{
	//Get Player
        VideoTranscoders::iterator it = videoTranscoders.find(videoTranscoderId);

        //If not found
        if (it==videoTranscoders.end())
                //Exit
                return Error("VideoTranscoder not found [%d]\n",videoTranscoderId);
        //Get it
        VideoTranscoder* videoTranscoder = it->second;

	//Execute
	return videoTranscoder->SetCodec(codec,size,fps,bitrate,intraPeriod,props);
}

int MediaSession::VideoTranscoderDelete(int videoTranscoderId)
{
	//Get Player
        VideoTranscoders::iterator it = videoTranscoders.find(videoTranscoderId);

        //If not found
        if (it==videoTranscoders.end())
                //Exit
                return Error("VideoTranscoder not found [%d]\n",videoTranscoderId);
        //Get it
        VideoTranscoder* videoTranscoder = it->second;

        //Remove from list
        videoTranscoders.erase(it);

	//End it
	videoTranscoder->End();

        //Relete videoMixer
        delete(videoTranscoder);

        return 1;
}

int MediaSession::VideoTranscoderAttachToEndpoint(int videoTranscoderId,int endpointId)
{
	//Get mixer
        VideoTranscoders::iterator it = videoTranscoders.find(videoTranscoderId);

        //If not found
        if (it==videoTranscoders.end())
                //Exit
                return Error("VideoTranscoder not found [%d]\n",videoTranscoderId);
        //Get it
        VideoTranscoder* videoTranscoder = it->second;

	//Get endpoint
        Endpoints::iterator itEnd = endpoints.find(endpointId);

        //If not found
        if (itEnd==endpoints.end())
                //Exit
                return Error("Endpoint not found [%d]\n",endpointId);

        //Get it
        Endpoint* endpoint = itEnd->second;

	//Log endpoint tag name
	Log("-VideoTranscoderAttachToEndpoint [transcoder:%ls,endpoint:%ls]\n",videoTranscoder->GetName().c_str(),endpoint->GetName().c_str());

	//Attach
	return videoTranscoder->Attach(endpoint->GetJoinable(MediaFrame::Video));
}

int MediaSession::VideoTranscoderDettach(int videoTranscoderId)
{
	//Get mixer
        VideoTranscoders::iterator it = videoTranscoders.find(videoTranscoderId);

        //If not found
        if (it==videoTranscoders.end())
                //Exit
                return Error("VideoTranscoder not found [%d]\n",videoTranscoderId);
        //Get it
        VideoTranscoder* videoTranscoder = it->second;

       //Attach
	return videoTranscoder->Dettach();
}
