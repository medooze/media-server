#include "PCAPTransportEmulator.h"
#include "VideoLayerSelector.h"


PCAPTransportEmulator::PCAPTransportEmulator() {}

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
		rtpMap[type] = codec;
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
		extMap[id] = ext;
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
			rtpMap[type] = codec;
			//Get rtx and fec
			BYTE rtx = it->GetProperty("rtx",0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				rtpMap[rtx] = VideoCodec::RTX;
				//Add the reverse one
				aptMap[rtx] = type;
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
		extMap[id] = ext;
	}
	
	//Clear extension
	extensions.clear();
}

bool PCAPTransportEmulator::AddIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-AddIncomingSourceGroup [rid:'%s',ssrc:%u,fec:%u,rtx:%u]\n",group->rid.c_str(),group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc && group->rid.empty())
		return Error("No media ssrc or rid defined, stream will not be added\n");
	

	//Check they are not already assigned
	if (group->media.ssrc && incoming.find(group->media.ssrc)!=incoming.end())
		return Error("-AddIncomingSourceGroup media ssrc already assigned");
	if (group->fec.ssrc && incoming.find(group->fec.ssrc)!=incoming.end())
		return Error("-AddIncomingSourceGroup fec ssrc already assigned");
	if (group->rtx.ssrc && incoming.find(group->rtx.ssrc)!=incoming.end())
		return Error("-AddIncomingSourceGroup rtx ssrc already assigned");

	/*
	//Add rid if any
	if (!group->rid.empty())
		rids[group->rid] = group;
	*/
	//Add it for each group ssrc
	if (group->media.ssrc)
		incoming[group->media.ssrc] = group;
	if (group->fec.ssrc)
		incoming[group->fec.ssrc] = group;
	if (group->rtx.ssrc)
		incoming[group->rtx.ssrc] = group;
	
	//Start distpaching
	group->Start();
	
	//Done
	return true;
}

bool PCAPTransportEmulator::RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-RemoveIncomingSourceGroup [ssrc:%u,fec:%u,rtx:%u]\n",group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc)
		return Error("No media ssrc defined, stream will not be removed\n");
	
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
	if (group->fec.ssrc)
		incoming.erase(group->fec.ssrc);
	if (group->rtx.ssrc)
		incoming.erase(group->rtx.ssrc);
	
	//Done
	return true;
}

bool PCAPTransportEmulator::Open(const char* filename)
{
	//Create new PCAP reader
	PCAPReader* reader = new PCAPReader();

	//Open pcap file
	if (!reader->Open(filename))
	{
		//Delte it
		delete reader;
		//Error
		return false;
	}
	
	//Set reader
	return SetReader(reader);
}

bool PCAPTransportEmulator::SetReader(UDPReader* reader)
{
	//Block condition
	ScopedLock lock(cond);
	
	//Double check
	if (!reader)
		//Error
		return false;
	
	//Store it
	this->reader.reset(reader);
	
	//Get first timestamp to start playing from
	first = reader->Seek(0)/1000;
	
	//Done
	return true;
}

bool PCAPTransportEmulator::Close()
{
	Debug(">PCAPTransportEmulator::Close()\n");
	
	//Block condition
	ScopedLock lock(cond);
	
	//Check we have reader
	if (!reader)
		return false;
	
	//Close reader
	return reader->Close();
}

bool PCAPTransportEmulator::Play()
{
	Debug(">PCAPTransportEmulator::Play()\n");
	
	//Block condition
	ScopedLock lock(cond);
	
	//Check we have reader
	if (!reader)
		return false;
	
	//Check thred
	if (!isZeroThread(thread))
	{
		//Not running
		running = false;
		
		//Cancel packets wait queue
		cond.Cancel();
		Debug("-PCAPTransportEmulator::Play() joining\n");
		//Wait thread to close
		pthread_join(thread,NULL);
		//Nulifi thread
		setZeroThread(&thread);
	}
	
	//We are running
	running = true;

	//Create thread
	createPriorityThread(&thread,[](void* par)->void*{
		//Block signals to avoid exiting on SIGUSR1
		blocksignals();

		//Obtenemos el parametro
		PCAPTransportEmulator *transport = (PCAPTransportEmulator *)par;

		//Run main loop
		transport->Run();

		//Exit
		return NULL;
	},this,0);
	
	Debug("<PCAPTransportEmulator::Play()\n");
	
	//Running
	return true;
}

uint64_t PCAPTransportEmulator::Seek(uint64_t time)
{
	Debug("-PCAPTransportEmulator::Seek() [time:%llu]\n",time);
	
	//Block condition
	ScopedLock lock(cond);
	
	//Check we have reader
	if (!reader)
		return 0;
	
	//Set first timestamp to start playing from
	first = time;
	
	//Seek it and return which is the first packet that will be played
	return reader->Seek(first*1000)/100;
}

bool PCAPTransportEmulator::Stop()
{
	Debug(">PCAPTransportEmulator::Stop()\n");
	
	//Check thred
	if (!isZeroThread(thread))
	{
		//Not running
		running = false;
		
		Debug("-PCAPTransportEmulator::Stop() | cond cancel()\n");
		
		//Cancel packets wait queue
		cond.Cancel();
		
		Debug("-PCAPTransportEmulator::Stop() | join\n");
		//Wait thread to close
		pthread_join(thread,NULL);
		//Nulifi thread
		setZeroThread(&thread);
	}
	
	Debug("<PCAPTransportEmulator::Stop()\n");
	
	return true;
}

int PCAPTransportEmulator::Run()
{
	Log(">PCAPTransportEmulator::Run() | [first:%llu,this:%p]\n",first,this);
	
	//Start play timestamp
	uint64_t ini  = getTime();
	uint64_t now = 0;
	
	//Restart condition
	cond.Reset();
	
	//Lock
	cond.Lock();

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
		uint64_t ts = reader->Next();
		
		//If we are at the end
		if (!ts)
		{
			Debug("-PCAPTransportEmulator::Run() | no more packets\n");
			//exit
			break;
		}
		
		//Get next packet from pcap
		uint8_t* buffer		= reader->GetUDPData();
		uint32_t bufferLen	= reader->GetUDPSize();
		
		//Check it is not RTCP
		if (RTCPCompoundPacket::IsRTCP(buffer,bufferLen))
		{
			//Debug
			//UltraDebug("-PCAPTransportEmulator::Run() | skipping rtcp\n");
			//Ignore this try again
			goto outher;
		}
	
		RTPHeader header;
		RTPHeaderExtension extension;

		//Parse RTP header
		uint32_t ini = header.Parse(buffer,bufferLen);

		//On error
		if (!ini)
		{
			//Debug
			Error("-PCAPTransportEmulator::Run() | Could not parse RTP header ini=%u len=%d\n",ini,bufferLen-ini);
			//Dump it
			Dump(buffer+ini,bufferLen-ini);
			//Ignore this try again
			goto outher;
		}
	
		//If it has extension
		if (header.extension)
		{
			//Parse extension
			int l = extension.Parse(extMap,buffer+ini,bufferLen-ini);
			//If not parsed
			if (!l)
			{
				///Debug
				Error("-PCAPTransportEmulator::Run() | Could not parse RTP header extension ini=%u len=%d\n",ini,bufferLen-ini);
				//Dump it
				Dump(buffer+ini,bufferLen-ini);
				//retry
				goto outher;
			}
			//Inc ini
			ini += l;
		}
	
		//Check size with padding
		if (header.padding)
		{
			//Get last 2 bytes
			WORD padding = get1(buffer,bufferLen-1);
			//Ensure we have enought size
			if (bufferLen-ini<padding)
			{
				///Debug
				Debug("-PCAPTransportEmulator::Run() | RTP padding is bigger than size [padding:%u,size%u]\n",padding,bufferLen);
				//Ignore this try again
				goto outher;
			}
			//Remove from size
			bufferLen -= padding;
		}

		//Check we have payload
		if (ini>=bufferLen)
		{
			///Debug
			UltraDebug("-PCAPTransportEmulator::Run() | Refusing to create a packet with empty payload [ini:%u,len:%u]\n",ini,bufferLen,ini);
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
		auto packet = std::make_shared<RTPPacket>(media,codec,header,extension);

		//Set the payload
		packet->SetPayload(buffer+ini,bufferLen-ini);

		//Set capture time in ms
		packet->SetTime(ts/1000);
		
		//Get the packet relative time in ns
		auto time = packet->GetTime() - first;
		
		//Get relative play times since start in ns
		now = getTimeDiff(ini)/1000;
		
		//Until the time of our packet has come
		while (now<time)
		{
			//Get when is the next packet to be played
			auto diff = time-now; 
			
			//UltraDebug("-PCAPTransportEmulator::Run() | waiting now:%llu next:%llu diff:%llu\n",now,time,diff);
				
			//Wait the difference
			if (!cond.Wait(diff))
			{
				Debug("-PCAPTransportEmulator::Run() | canceled\n");
				//Check if we have been stoped
				goto outher;
			}
			
			//Get relative play times since start in ns
			now = getTimeDiff(ini);
		}
		
		//Get sssrc
		DWORD ssrc = packet->GetSSRC();
		
		//Get the incouming source
		auto it = incoming.find(ssrc);
				
		//If not found
		if (it==incoming.end())
		{
			//error
			Debug("-PCAPTransportEmulator::Run() | Unknowing source for ssrc [%u]\n",ssrc);
			//Continue
			continue;
		}
		
		//Get group
		RTPIncomingSourceGroup *group = it->second;

		//TODO:support rids

		//Ensure it has a group
		if (!group)	
		{
			//error
			Debug("-PCAPTransportEmulator::Run()| Unknowing group for ssrc [%u]\n",ssrc);
			//Skip
			continue;
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
				Debug("-PCAPTransportEmulator::Run() | RTP RTX packet apt type unknown [%d]\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType());
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
			//Set codec
			packet->SetCodec(codec);
			packet->SetPayloadType(apt);
	
		} else if (ssrc==group->fec.ssrc)  {
			UltraDebug("-Flex fec\n");
			//Ensure that it is a FEC codec
			if (codec!=VideoCodec::FLEXFEC)
				//error
				Debug("-PCAPTransportEmulator::Run()| No FLEXFEC codec on fec sssrc:%u type:%d codec:%d\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType(),packet->GetSSRC());
			//DO NOTHING with it yet
			continue;
		}	
		
		//Add packet and see if we have lost any in between
		int lost = group->AddPacket(packet);

		//Check if it was rejected
		if (lost<0)
		{
			UltraDebug("-PCAPTransportEmulator::Run()| Dropped packet [ssrc:%u,seq:%d]\n",packet->GetSSRC(),packet->GetSeqNum());
			//Increase rejected counter
			source->dropPackets++;
		} 
	}

	//Unlock
	cond.Unlock();
	
	Log("<PCAPTransportEmulator::Run()\n");
	
	return 0;
}
