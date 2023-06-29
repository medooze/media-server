/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPDepacketizer.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:55
 */

#include "rtp/RTPDepacketizer.h"
#include "audio.h"
#include "bitstream.h"
#include "opus/opusdepacketizer.h"
#include "h264/h264depacketizer.h"
#include "h265/H265Depacketizer.h"
#include "vp8/vp8depacketizer.h"
#include "vp9/VP9Depacketizer.h"
#include "av1/AV1Depacketizer.h"

RTPDepacketizer* RTPDepacketizer::Create(MediaFrame::Type mediaType,DWORD codec)
{
	Debug("-RTPDepacketizer::Create() | Creating depacketizer for [media:%s,codec:%s]\n",MediaFrame::TypeToString(mediaType),GetNameForCodec(mediaType,codec));
	
	 switch (mediaType)
	 {
		 case MediaFrame::Video:
			 //Depending on the codec
			 switch((VideoCodec::Type)codec)
			 {
				 case VideoCodec::H264:
					 return new H264Depacketizer();
				 case VideoCodec::H265:
					 return new H265Depacketizer();
				 case VideoCodec::VP8:
					 return new VP8Depacketizer();
				 case VideoCodec::VP9:
					 return new VP9Depacketizer();
				 case VideoCodec::AV1:
					 return new AV1Depacketizer();
				 default:
					Error("-RTPDepacketizer::Create we don't have an RTP depacketizer for [media:%s,codec:%s]\n",MediaFrame::TypeToString(mediaType),GetNameForCodec(mediaType,codec));
			 }
			 break;
		 case MediaFrame::Audio:
		 {
			 //GEt codec
			 AudioCodec::Type  audioCodec = (AudioCodec::Type)codec;
			 //Depending on the codec
			 switch(audioCodec)
			 {
				case AudioCodec::GSM:
				case AudioCodec::PCMA:
				case AudioCodec::PCMU:
					//Dummy depacketizer
					return new DummyAudioDepacketizer(audioCodec);
				case AudioCodec::SPEEX16:
				case AudioCodec::G722:
					//Dummy depacketizer
					return new DummyAudioDepacketizer(audioCodec,16000);
				case AudioCodec::OPUS:
					//Dummy depacketizer
					return new OpusDepacketizer();
				 default:
					//Dummy depacketizer
					return new DummyAudioDepacketizer(audioCodec);
			 }
			 break;
		 }
		 default:
			 Error("-RTPDepacketizer::Create unknown media type\n");
	 }
	 return NULL;
}
