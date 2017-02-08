/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPPacket.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:58
 */

#include "rtp/RTCPPacket.h"
#include "log.h"

void RTCPPacket::Dump()
{
	Debug("\t[RTCPpacket type=%s size=%d/]\n",TypeToString(type),GetSize());
}
