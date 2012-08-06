/* 
 * File:   MediaSession.h
 * Author: Sergio
 *
 * Created on 6 de septiembre de 2011, 15:55
 */

#ifndef MEDIASESSION_H
#define	MEDIASESSION_H

#include "config.h"
#include "media.h"
#include "codecs.h"
#include "video.h"
#include "Player.h"
#include "Recorder.h"
#include "Endpoint.h"
#include "AudioMixerResource.h"
#include "VideoMixerResource.h"

class MediaSession : public Player::Listener
{
public:
	class Listener
	{
	public:
		virtual void onPlayerEndOfFile(MediaSession *sess, Player* player,int playerId, void *param) = 0;
	};
public:
	MediaSession(std::wstring tag);
	~MediaSession();

	void SetListener(MediaSession::Listener *listener,void* param);

	int Init();
	int End();

	//Player management
	int PlayerCreate(std::wstring tag);
	int PlayerDelete(int playerId);
	//Player functionality
	int PlayerOpen(int playerId,const char* filename);
	int PlayerPlay(int playerId);
	int PlayerSeek(int playerId,QWORD time);
	int PlayerStop(int playerId);
	int PlayerClose(int playerId);

	//Recorder management
	int RecorderCreate(std::wstring tag);
	int RecorderDelete(int recorderId);
	//Recorder functionality
	int RecorderRecord(int recorderId,const char* filename);
	int RecorderStop(int recorderId);

	//Join other objects
	int RecorderAttachToEndpoint(int recorderId,int endpointId,MediaFrame::Type media);
	int RecorderAttachToAudioMixerPort(int recorderId,int mixerId,int portId);
	int RecorderAttachToVideoMixerPort(int recorderId,int mixerId,int portId);
	int RecorderDettach(int recorderId,MediaFrame::Type media);
	

	//Endpoint management
	int EndpointCreate(std::wstring name,bool audioSupported,bool videoSupported,bool textSupported);
	int EndpointDelete(int endpointId);
	//Endpoint Video functionality
	int EndpointStartSendingVideo(int endpointId,char *sendVideoIp,int sendVideoPort,VideoCodec::RTPMap& rtpMap);
	int EndpointStopSendingVideo(int endpointId);
	int EndpointStartReceivingVideo(int endpointId,VideoCodec::RTPMap& rtpMap);
	int EndpointStopReceivingVideo(int endpointId);
	//Endpoint Audio functionality
	int EndpointStartSendingAudio(int endpointId,char *sendAudioIp,int sendAudioPort,AudioCodec::RTPMap& rtpMap);
	int EndpointStopSendingAudio(int endpointId);
	int EndpointStartReceivingAudio(int endpointId,AudioCodec::RTPMap& rtpMap);
	int EndpointStopReceivingAudio(int endpointId);

	int EndpointStartSendingText(int endpointId,char *sendTextIp,int sendTextPort,TextCodec::RTPMap& rtpMap);
	int EndpointStopSendingText(int endpointId);
	int EndpointStartReceivingText(int endpointId,TextCodec::RTPMap& rtpMap);
	int EndpointStopReceivingText(int endpointId);
	int EndpointRequestUpdate(int endpointId,MediaFrame::Type media);
	//Attach intput to
	int EndpointAttachToPlayer(int endpointId,int playerId,MediaFrame::Type media);
	int EndpointAttachToEndpoint(int endpointId,int sourceId,MediaFrame::Type media);
	int EndpointAttachToAudioMixerPort(int endpointId,int mixerId,int portId);
	int EndpointDettach(int endpointId,MediaFrame::Type media);
	int EndpointAttachToVideoMixerPort(int endpointId,int mixerId,int portId);

	//AudioMixer management
	int AudioMixerCreate(std::wstring tag);
	int AudioMixerDelete(int mixerId);
	//AudioMixer port management
	int AudioMixerPortCreate(int mixerId,std::wstring tag);
	int AudioMixerPortSetCodec(int mixerId,int portId,AudioCodec::Type codec);
	int AudioMixerPortDelete(int mixerId,int portId);
	//Filters type: 0=pre 1=post
	//int AudioMixerPortAddFilter(int mixerId,int portId,int type,...);
	//int AudioMixerPortUpdatedFilter(int mixerId,int portId,...);
	//int AudioMixerPortDeleteFilter(int mixerId,int portId,int filterId);
	//int AudioMixerPortClearFilters(int mixerId,int portId);
	//Port Attach  to
	int AudioMixerPortAttachToEndpoint(int mixerId,int portId,int endpointId);
	int AudioMixerPortAttachToPlayer(int mixerId,int portId,int playerId);
	int AudioMixerPortDettach(int mixerId,int portId);

	//Video Mixer management
	int VideoMixerCreate(std::wstring tag);
	int VideoMixerDelete(int mixerId);
	//Video mixer port management
	int VideoMixerPortCreate(int mixerId,std::wstring tag,int mosiacId);
	int VideoMixerPortSetCodec(int mixerId,int portId,VideoCodec::Type codec,int size,int fps,int bitrate,int intraPeriod);
	int VideoMixerPortDelete(int mixerId,int porId);
	int VideoMixerPortAttachToEndpoint(int mixerId,int portId,int endpointId);
	int VideoMixerPortAttachToPlayer(int mixerId,int portId,int playerId);
	int VideoMixerPortDettach(int mixerId,int portId);
	//Video mixer mosaic management
	int VideoMixerMosaicCreate(int mixerId,Mosaic::Type comp,int size);
	int VideoMixerMosaicDelete(int mixerId,int portId);
	int VideoMixerMosaicSetSlot(int mixerId,int mosaicId,int num,int portId);
	int VideoMixerMosaicSetCompositionType(int mixerId,int mosaicId,Mosaic::Type comp,int size);
	int VideoMixerMosaicSetOverlayPNG(int mixerId,int mosaicId,const char* overlay);
	int VideoMixerMosaicResetSetOverlay(int mixerId,int mosaicId);
	int VideoMixerMosaicAddPort(int mixerId,int mosaicId,int portId);
	int VideoMixerMosaicRemovePort(int mixerId,int mosaicId,int portId);

	//Events
	virtual void onEndOfFile(Player *player,void* param);

	//Getters
	std::wstring& GetTag() { return tag;	}
private:
	typedef std::map<int,Endpoint*> Endpoints;
	typedef std::map<int,Recorder*> Recorders;
	typedef std::map<int,Player*> Players;
	typedef std::map<int,AudioMixerResource*> AudioMixers;
	typedef std::map<int,VideoMixerResource*> VideoMixers;
private:
	std::wstring tag;
	
	MediaSession::Listener *listener;
	void* param;

	Endpoints endpoints;
	int maxEndpointId;

	Players players;
	int maxPlayersId;

	Recorders recorders;
	int maxRecordersId;

	AudioMixers	audioMixers;
	int maxAudioMixerId;

	VideoMixers	videoMixers;
	int maxVideoMixerId;
 
};

#endif	/* MEDIASESSION_H */

