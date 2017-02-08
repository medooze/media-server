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
#include "h264/h264depacketizer.h"
#include "vp8/vp8depacketizer.h"

RTPDepacketizer* RTPDepacketizer::Create(MediaFrame::Type mediaType,DWORD codec)
{
	 switch (mediaType)
	 {
		 case MediaFrame::Video:
			 //Depending on the codec
			 switch((VideoCodec::Type)codec)
			 {
				 case VideoCodec::H264:
					 return new H264Depacketizer();
				 case VideoCodec::VP8:
					 return new VP8Depacketizer();                
			 }
			 break;
		 case MediaFrame::Audio:
			 //Dummy depacketizer
			 return new DummyAudioDepacketizer(codec);
			 break;
	 }
	 return NULL;
}