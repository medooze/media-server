#include "PCAPTransportEmulator.h"
#include "VideoLayerSelector.h"


PCAPTransportEmulator::PCAPTransportEmulator()
{
	loop.Start(FD_INVALID);
}

PCAPTransportEmulator::~PCAPTransportEmulator() 
{
	Stop();
}

void PCAPTransportEmulator::SetRemoteProperties(const Properties& properties)
{
	//For each property
	for (Properties::const_iterator it=properties.begin();it!=properties.end();++it)
		Debug("-PCAPTransportEmulator::SetRemoteProperties | Setting property [%s:%s]\n",it->first.c_str(),it->second.c_str());
	
	std::vector<Properties> codecs;
	std::vector<Properties> extensions;
	
	//Cleant maps
	rtpMap.clear();
	extMap.clear();
	aptMap.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("audio.codecs",codecs);

	//For each codec
	for (auto it = codecs.begin(); it!=codecs.end(); ++it)
	{
		//Get codec by name
		AudioCodec::Type codec = AudioCodec::GetCodecForName(it->GetProperty("codec"));
		//Get codec type
		BYTE type = it->GetProperty("pt",0);
		//ADD it
		rtpMap.SetCodecForType(type, codec);
	}
	
	//Clear codecs
	codecs.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("audio.ext",extensions);
	
	//For each codec
	for (auto it = extensions.begin(); it!=extensions.end(); ++it)
	{
		//Get Extension for name
		RTPHeaderExtension::Type ext = RTPHeaderExtension::GetExtensionForName(it->GetProperty("uri"));
		//Get extension id
		BYTE id = it->GetProperty("id",0);
		//ADD it
		extMap.SetCodecForType(id, ext);
	}
	
	//Clear extension
	extensions.clear();
	
	//Get video codecs
	properties.GetChildrenArray("video.codecs",codecs);
	
	//For each codec
	for (auto it = codecs.begin(); it!=codecs.end(); ++it)
	{
		//Get codec by name
		VideoCodec::Type codec = VideoCodec::GetCodecForName(it->GetProperty("codec"));
		//Get codec type
		BYTE type = it->GetProperty("pt",0);
		//Check
		if (type && codec!=VideoCodec::UNKNOWN)
		{
			//ADD it
			rtpMap.SetCodecForType(type, codec);
			//Get rtx
			BYTE rtx = it->GetProperty("rtx",0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				rtpMap.SetCodecForType(rtx, VideoCodec::RTX);
				//Add the reverse one
				aptMap.SetCodecForType(rtx, type);
			}
		}
	}
	
	//Clear codecs
	codecs.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("video.ext",extensions);
	
	//For each codec
	for (auto it = extensions.begin(); it!=extensions.end(); ++it)
	{
		//Get Extension for name
		RTPHeaderExtension::Type ext = RTPHeaderExtension::GetExtensionForName(it->GetProperty("uri"));
		//Get extension id
		BYTE id = it->GetProperty("id",0);
		//ADD it
		extMap.SetCodecForType(id, ext);
	}
	
	//Clear extension
	extensions.clear();
}

bool PCAPTransportEmulator::AddIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-PCAPTransportEmulator::AddIncomingSourceGroup() [rid:'%s',ssrc:%u,rtx:%u]\n",group->rid.c_str(),group->media.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc && group->rid.empty())
	{
		//Add to unknown groups
		unknow[group->type] = group;
	} else {
		//Check they are not already assigned
		if (group->media.ssrc && incoming.find(group->media.ssrc)!=incoming.end())
			return Error("-PCAPTransportEmulator::AddIncomingSourceGroup() media ssrc already assigned");
		if (group->rtx.ssrc && incoming.find(group->rtx.ssrc)!=incoming.end())
			return Error("-PCAPTransportEmulator::AddIncomingSourceGroup() rtx ssrc already assigned");

		/*
		//Add rid if any
		if (!group->rid.empty())
			rids[group->rid] = group;
		*/
		//Add it for each group ssrc
		if (group->media.ssrc)
			incoming[group->media.ssrc] = group;
		if (group->rtx.ssrc)
			incoming[group->rtx.ssrc] = group;
	}

	//Set RTX supported flag only for video
	group->SetRTXEnabled(group->type == MediaFrame::Video);
	
	//Start distpaching
	group->Start();
	
	//Done
	return true;
}

bool PCAPTransportEmulator::RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-PCAPTransportEmulator::RemoveIncomingSourceGroup() [ssrc:%u,rtx:%u]\n",group->media.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc)
		return Error("-PCAPTransportEmulator::RemoveIncomingSourceGroup() No media ssrc defined, stream will not be removed\n");
	
	//Stop distpaching
	group->Stop();
	
/*
	//Remove rid if any
	if (!group->rid.empty())
		rids.erase(group->rid);
*/	
	//Remove it from each ssrc
	if (group->media.ssrc)
		incoming.erase(group->media.ssrc);
	if (group->rtx.ssrc)
		incoming.erase(group->rtx.ssrc);
	
	//Done
	return true;
}

bool PCAPTransportEmulator::Open(const char* filename)
{
	Debug("-PCAPTransportEmulator::Open() [file:%s]\n", filename);

	//Create new PCAP reader
	PCAPReader* reader = new PCAPReader();

	//Open pcap file
	if (!reader->Open(filename))
	{
		//Delte it
		delete reader;
		//Error
		return Error("-PCAPTransportEmulator::Open() | could not open pcap file\n");
	}
	
	//Set reader
	return SetReader(reader);
}

bool PCAPTransportEmulator::SetReader(UDPReader* reader)
{
	//Double check
	if (!reader)
		//Error
		return false;
	
	//Stop
	Stop();
	
	//Store it
	this->reader.reset(reader);
	
	//Get first timestamp to start playing from
	first = reader->Seek(0)/1000;

	//Dispatch timers & tasks
	loop.Start(FD_INVALID);
	
	//Done
	return true;
}

bool PCAPTransportEmulator::Close()
{
	Debug(">PCAPTransportEmulator::Close()\n");
	
	//Check we have reader
	if (!reader)
		return false;
	
	//Stop
	Stop();
	
	//Close reader
	return reader->Close();
}

bool PCAPTransportEmulator::Play()
{
	Debug(">PCAPTransportEmulator::Play()\n");
	
	//Check we have reader
	if (!reader)
		return false;
	
	//Stop running loop
	loop.Stop();
	
	//We are running now
	running = true;

	//Create thread
	loop.Start([this](){ Run(); });
	
	Debug("<PCAPTransportEmulator::Play()\n");
	
	//Running
	return true;
}

uint64_t PCAPTransportEmulator::Seek(uint64_t time)
{
	Debug("-PCAPTransportEmulator::Seek() [time:%llu]\n",time);
	
	//Check we have reader
	if (!reader)
		return 0;

	//Stop
	Stop();
	
	//Store start time
	first = time;
	
	//Seek it and return which is the first packet that will be played
	return reader->Seek(time*1000)/1000;
}

bool PCAPTransportEmulator::Stop()
{
	Debug(">PCAPTransportEmulator::Stop()\n");
	
	//Not running
	running = false;
	
	//Check thread
	loop.Stop();
	
	Debug("<PCAPTransportEmulator::Stop()\n");
	
	//Dispatch timers & tasks
	loop.Start(FD_INVALID);

	return true;
}

int PCAPTransportEmulator::Run()
{
	Log(">PCAPTransportEmulator::Run() | [first:%llu,this:%p]\n",first,this);
	
	//Start play timestamp
	uint64_t ini  = getTime();
	uint64_t now = 0;
	
	//Run until canceled
outher:	while(running)
	{
		//ensure we have reader
		if (!reader)
		{
			Debug("-PCAPTransportEmulator::Run() | no reader\n");
			//exit
			break;
		}
			
		//Get packet
		uint64_t ts = reader->Next()/1000;
		
		//If we are at the end
		if (!ts)
		{
			Debug("-PCAPTransportEmulator::Run() | no more packets\n");
			//exit
			break;
		}
		
		//Get next packet from pcap
		uint8_t* data = reader->GetUDPData();
		uint32_t size = reader->GetUDPSize();
		
		//Check it is not RTCP
		if (RTCPCompoundPacket::IsRTCP(data,size))
		{
			//Parse it
			auto rtcp = RTCPCompoundPacket::Parse(data, size);

			//Check packet
			if (!rtcp)
			{
				//Debug
				Debug("-DTLSICETransport::onData() | RTCP wrong data\n");
				//Dump it
				::Dump(data, size);
				//Next
				goto outher;
			}

			// For each packet
			for (DWORD i = 0; i < rtcp->GetPacketCount(); i++)
			{
				//Get pacekt
				auto packet = rtcp->GetPacket(i);
				//Check packet type
				switch (packet->GetType())
				{
					case RTCPPacket::SenderReport:
					{
						//Get sender report
						auto sr = std::static_pointer_cast<RTCPSenderReport>(packet);

						//Get ssrc
						DWORD ssrc = sr->GetSSRC();

						//Get source
						RTPIncomingSource* source = GetIncomingSource(ssrc);

						//If not found
						if (!source)
						{
							Warning("-DTLSICETransport::onRTCP() | Could not find incoming source for RTCP SR [ssrc:%u]\n", ssrc);
							rtcp->Dump();
							continue;
						}

						//Update source
						source->Process(ts, sr);
						break;
					}
					case RTCPPacket::Bye:
					{
						//Get bye
						auto bye = std::static_pointer_cast<RTCPBye>(packet);
						//For each ssrc
						for (auto& ssrc : bye->GetSSRCs())
						{
							//Get media
							RTPIncomingSourceGroup* group = GetIncomingSourceGroup(ssrc);

							//Debug
							Debug("-DTLSICETransport::onRTCP() | Got BYE [ssrc:%u,group:%p,this:%p]\n", ssrc, group, this);

							//If found
							if (group)
								//Reset it
								group->Bye(ssrc);
						}
						break;
					}
					default:
					{
						//Ignore
					}
				}
			}

			//Next
			goto outher;
		}
	
		RTPHeader header;
		RTPHeaderExtension extension;

		//Parse RTP header
		uint32_t len = header.Parse(data,size);

		//On error
		if (!len)
		{
			//Debug
			Error("-PCAPTransportEmulator::Run() | Could not parse RTP header ini=%u len=%d\n",len,size-len);
			//Dump it
			Dump(data+len,size-len);
			//Ignore this try again
			goto outher;
		}
	
		//If it has extension
		if (header.extension)
		{
			//Parse extension
			int l = extension.Parse(extMap,data+len,size-len);
			//If not parsed
			if (!l)
			{
				///Debug
				Error("-PCAPTransportEmulator::Run() | Could not parse RTP header extension ini=%u len=%d\n",len,size-len);
				//Dump it
				Dump(data+len,size-len);
				//retry
				goto outher;
			}
			//Inc ini
			len += l;
		}
	
		//Check size with padding
		if (header.padding)
		{
			//Get last 2 bytes
			WORD padding = get1(data,size-1);
			//Ensure we have enought size
			if (size-len<padding)
			{
				///Debug
				Debug("-PCAPTransportEmulator::Run() | RTP padding is bigger than size [padding:%u,size%u]\n",padding,size);
				//Ignore this try again
				goto outher;
			}
			//Remove from size
			size -= padding;
		}

		//Check we have payload
		if (len>=size)
		{
			///Debug
			UltraDebug("-PCAPTransportEmulator::Run() | Refusing to create a packet with empty payload [ini:%u,len:%u]\n",size,len);
			//Ignore this try again
			goto outher;
		}

		//Get initial codec
		BYTE codec = rtpMap.GetCodecForType(header.payloadType);

		//Check codec
		if (codec==RTPMap::NotFound)
		{
			//Error
			Error("-PCAPTransportEmulator::Run() | RTP packet type unknown [%d]\n",header.payloadType);
			//retry
			goto outher;
		}

		//Get media
		MediaFrame::Type media = GetMediaForCodec(codec);

		//Create normal packet
		auto packet = std::make_shared<RTPPacket>(media,codec,header,extension, ts);

		//Set the payload
		packet->SetPayload(data+len,size-len);
		
		//Get the packet relative time in ns
		auto time = packet->GetTime() - first;
		
		//Get relative play times since start in ns
		now = getTimeDiff(ini)/1000;
		
		//Until the time of our packet has come
		while (now<time)
		{
			//Get when is the next packet to be played
			uint64_t diff = time-now; 
			
			//UltraDebug("-PCAPTransportEmulator::Run() | waiting now:%llu next:%llu diff:%llu first:%llu, ts:%llu\n",now,time,diff,first,ts);
				
			//Wait the difference
			loop.Run(std::chrono::milliseconds(diff));

			//Check if we have been stoped
			if (!running)
				goto outher;
			
			//Get relative play times since start in ns
			now = getTimeDiff(ini)/1000;
		}
		
		//Get sssrc
		DWORD ssrc = packet->GetSSRC();
		
		//Get group
		RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);

		//TODO:support rids

		//Ensure it has a group
		if (!group)	
		{
			//If we have an unknown group for that kind
			auto it = unknow.find(media);
			//If not found
			if (it==unknow.end())
			{
				//error
				Debug("-PCAPTransportEmulator::Run()| Unknown group for ssrc [%u]\n",ssrc);
				//Skip
				continue;
			}
			//Get group
			group = it->second;
			
			//Check if it is rtx or media
			if (media==MediaFrame::Video && codec==VideoCodec::RTX)
			{
				//Log
				Debug("-PCAPTransportEmulator::Run()| Assigning rtx ssrc [%u] to group [%p]\n", ssrc, group);
				//Set rtx ssrc
				group->rtx.ssrc = ssrc;
				incoming[group->rtx.ssrc] = group;
			} else {
				//Log
				Debug("-PCAPTransportEmulator::Run()| Assigning media ssrc [%u] to group [%p]\n", ssrc, group);
				//Set media ssrc
				group->media.ssrc = ssrc;
				incoming[group->media.ssrc] = group;
			}
		}

		//UltraDebug("-PCAPTransportEmulator::Run() | Got RTP on media:%s sssrc:%u seq:%u pt:%u codec:%s rid:'%s'\n",MediaFrame::TypeToString(group->type),ssrc,packet->GetSeqNum(),packet->GetPayloadType(),GetNameForCodec(group->type,codec),group->rid.c_str());

		//Process packet and get source
		RTPIncomingSource* source = group->Process(packet);

		//Ensure it has a source
		if (!source)
		{
			//error
			Debug("-PCAPTransportEmulator::Run()| Group does not contain ssrc [%u]\n",ssrc);
			//Continue
			continue;
		}
		
		//If it was an RTX packet
		if (ssrc==group->rtx.ssrc) 
		{
			//Ensure that it is a RTX codec
			if (packet->GetCodec()!=VideoCodec::RTX)
			{
				//error
				Debug("-PCAPTransportEmulator::Run()| No RTX codec on rtx sssrc:%u type:%d codec:%d\n",packet->GetSSRC(),packet->GetPayloadType(),packet->GetCodec());
				//Skip
				continue;
			}

			//Find apt type
			auto apt = aptMap.GetCodecForType(packet->GetPayloadType());
			//Find codec 
			codec = rtpMap.GetCodecForType(apt);
			//Check codec
			if (codec==RTPMap::NotFound)
			{
				//Error
				Debug("-PCAPTransportEmulator::Run() | RTP RTX packet apt type unknown [%s %d]\n",MediaFrame::TypeToString(packet->GetMediaType()),packet->GetPayloadType());
				//Skip
				continue;
			}

			//Remove OSN and restore seq num
			if (!packet->RecoverOSN())
			{
				//error
				Debug("-PCAPTransportEmulator::Run() | RTX not enough data len:%d\n",packet->GetMediaLength());
				//Skip
				continue;
			}
			
			//Set original ssrc
			packet->SetSSRC(group->media.ssrc);
			//Set corrected seq num cycles
			packet->SetSeqCycles(group->media.RecoverSeqNum(packet->GetSeqNum()));
			//Set corrected timestamp cycles
			packet->SetTimestampCycles(group->media.RecoverTimestamp(packet->GetTimestamp()));
			//Set codec
			packet->SetCodec(codec);
			packet->SetPayloadType(apt);
			//TODO: Move from here
			VideoLayerSelector::GetLayerIds(packet);
		}
		
		//Log("-%llu(%lld) %s seqNum:%llu(%u) mark:%d\n",ini+now,now,MediaFrame::TypeToString(group->type),packet->GetExtSeqNum(),packet->GetSeqNum(),packet->GetMark());
		
		//Add packet and see if we have lost any in between
		int lost = group->AddPacket(packet,size,ts);

		//Check if it was rejected
		if (lost<0)
		{
			UltraDebug("-PCAPTransportEmulator::Run()| Dropped packet [ssrc:%u,seq:%d]\n",packet->GetSSRC(),packet->GetSeqNum());
			//Increase rejected counter
			source->dropPackets++;
		} else if (lost > 0) {
			UltraDebug("-PCAPTransportEmulator::Run()| lost packets [ssrc:%u,seq:%d;lost:%d]\n", packet->GetSSRC(), packet->GetSeqNum(),lost);
		}
	}

	//Run
	if (running)
		//Run event loop normaly
		loop.Run();
			
	Log("<PCAPTransportEmulator::Run()\n");
	
	return 0;
}



RTPIncomingSourceGroup* PCAPTransportEmulator::GetIncomingSourceGroup(DWORD ssrc)
{
	//Get the incouming source
	auto it = incoming.find(ssrc);

	//If not found
	if (it == incoming.end())
		//Not found
		return NULL;

	//Get source froup
	return it->second;
}

RTPIncomingSource* PCAPTransportEmulator::GetIncomingSource(DWORD ssrc)
{
	//Get the incouming source
	auto it = incoming.find(ssrc);

	//If not found
	if (it == incoming.end())
		//Not found
		return NULL;

	//Get source
	return it->second->GetSource(ssrc);

}