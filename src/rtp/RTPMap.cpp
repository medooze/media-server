/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPMap.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:50
 */

#include "rtp/RTPMap.h"
#include "codecs.h"

void RTPMap::Dump(MediaFrame::Type media) const
{
	Debug("[RTPMap]\n");
	//Try to find it in the map
	for (auto it = data.begin(); it!=data.end(); ++it)
		Debug("\t[Codec name=%s payload=%d/]\n",GetNameForCodec(media,it->second),it->first);
	Debug("[/RTPMap]\n");
}
