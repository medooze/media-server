/* 
 * File:   fecdecoder.cpp
 * Author: Sergio
 * 
 * Created on 6 de febrero de 2013, 10:30
 */

#include <set>

#include "fecdecoder.h"
#include "codecs.h"
#include "bitstream.h"

void DumpBits(QWORD val)
{
	char str[65];
	BYTE aux[8];
	set8(aux,0,val);
	BitReader r(aux,8);
	for (int i=0;i<64;i++)
		str[i] = r.Get(1) ? '1' : '0';
	str[64]=0;
	Debug("0x%.16llx %s\n",val,str);
}

FECDecoder::FECDecoder()
{
}

FECDecoder::~FECDecoder()
{
	//Delete all media
	for(RTPOrderedPackets::iterator it = medias.begin(); it!=medias.end(); it++)
		//Delete rtp packet
		delete (it->second);
	//Delete all media
	for(FECOrderedData::iterator it = codes.begin(); it!=codes.end(); it++)
		//Delete rtp packet
		delete (it->second);
}

void FECDecoder::AddFECData(BYTE* buffer,DWORD size)
{
	//Create new FEC data
	FECData *fec = new FECData(buffer,size);

	Debug("Fec data base:%d long:%d mask:%llx size:%d\n",fec->GetBaseExtSeq(),fec->GetLongMask(),fec->GetLevel0Mask(),fec->GetLevel0Size());
	Dump(buffer,18);
	DumpBits(fec->GetLevel0Mask());

	//Check if it was ok
	if (fec)
		//Append it
		codes[fec->GetBaseExtSeq()] = fec;
	
	//Delete
	RTPOrderedPackets::iterator it = medias.find(fec->GetBaseExtSeq());
	if (it!=medias.end())
	{
		Dump(it->second->GetData(),32);
		delete(it->second);
		medias.erase(it);
	}
}

void FECDecoder::AddPacket(RTPTimedPacket* packet)
{
	//Check if it is a redundant packet
	if (packet->GetCodec()==VideoCodec::RED)
	{
		//Get the redundant packet
		RTPRedundantPacket *red = (RTPRedundantPacket *)packet;
		
		//Check primary redundant type
		if (red->GetPrimaryCodec()==VideoCodec::ULPFEC)
			//Add fec data to list
			AddFECData(red->GetPrimaryPayloadData(),red->GetPrimaryPayloadSize());
		else
			//Add media packet,
			medias[packet->GetExtSeqNum()] = red->CreatePrimaryPacket();

		//For each redundant data
		for (int i=0;i<red->GetRedundantCount();++i)
			//Check if it is a FEC pacekt
			if (red->GetRedundantCodec(i)==VideoCodec::ULPFEC)
				//Create new FEC data
				AddFECData(red->GetRedundantPayloadData(i),red->GetRedundantPayloadSize(i));
	} else if (packet->GetCodec()==VideoCodec::ULPFEC) {
		//Create new FEC data
		 AddFECData(packet->GetMediaData(),packet->GetMediaLength());
	} else {
		//Add media packet
		medias[packet->GetExtSeqNum()] = packet->Clone();
	}
	//Get last seq number
	DWORD seq = medias.rbegin()->first;
	
	//Remove unused packets
	RTPOrderedPackets::iterator it = medias.begin();
	//Delete everything until seq-63
	while (it->first<seq-63 && it!=medias.end())
	{
		//Delete rtp packet
		delete (it->second);
		//Erase it
		medias.erase(it++);
	}
	
	//Now clean recovery codes
	FECOrderedData::iterator it2 = codes.begin();
	//Check base sequence
	while (it2->first<seq-63 && it2!=codes.end())
	{
		//Delete object
		delete(it2->second);
		//Erase and move to next
		codes.erase(it2++);
	}
}

RTPTimedPacket* FECDecoder::Recover()
{
	BYTE aux[8];
	QWORD lostMask = 0;

	//Check we have media pacekts
	if (!medias.size())
		//Exit
		return NULL;
	//Get First packet
	RTPPacket* first = medias.begin()->second;
	//Get the SSRC
	DWORD ssrc = first->GetSSRC();
	//Get first media packet
	DWORD minSeq = first->GetExtSeqNum();
	//Iterator on seq
	DWORD lastSeq = minSeq;

	//Set to 0
	memset(aux,8,0);
	//Create writter
	BitWritter w(aux,8);
	//vector of lost pacekt seq
	std::vector<DWORD> losts;

	//For each media packet
	for (RTPOrderedPackets::iterator it=medias.begin();it!=medias.end();++it)
	{
		//Get seq
		DWORD cur = it->first;
		//Insert lost
		for (DWORD i=lastSeq+1;i<cur;++i)
		{
			//set mask bit to not present
			w.Put(1,0);
			//Add to the vecotr
			losts.push_back(i);
		}
		//Set last seq
		lastSeq = cur;
		//set mask bit to present
		w.Put(1,1);
	}

	//End it
	w.Flush();

	//Get mask
	lostMask = get8(aux,0);

	//Check we have lost pacekts
	if (!losts.size())
		//Exit
		return NULL;

	//For each lost packet
	for(std::vector<DWORD>::iterator it=losts.begin();it!=losts.end();++it)
	{
		//Get lost packet sequence
		DWORD seq = *it;

		Debug("-lost seq %d %llx %d %d\n",seq,lostMask,minSeq,lastSeq);
		
		//Search FEC packets associated this media packet
		for (FECOrderedData::iterator it2 = codes.begin();it2!=codes.end();++it2)
		{
			//Get FEC packet
			FECData *fec = it2->second;
			//Debug
			Debug("-fec %d %llx\n",fec->GetBaseExtSeq(),fec->GetLevel0Mask());
			
			//Check if it is associated with this media pacekt in level 0
			if (!fec->IsProtectedAtLevel0(seq))
				//Next
				continue;
			Debug("---------seq %d base %d\n",seq,fec->GetBaseExtSeq());
			//Get the seq difference between fec data and the media
			// fec seq has to be <= media seq it fec data protect media data)
			DWORD diff = seq-fec->GetBaseExtSeq();
			//Shit mask of the lost packets to check the present ones from the base seq
			QWORD mediaMask = lostMask << (fec->GetBaseExtSeq()-minSeq);
			//Remove lost packet bit from the fec mask
			QWORD fecMask = fec->GetLevel0Mask() & ~(((QWORD)1)<<(64-diff-1));

			Debug("-fe masks %llx diff %d\n",fecMask,diff);
			DumpBits(fec->GetLevel0Mask());
			DumpBits(fecMask);
			Debug("-media masks %llx %d\n",mediaMask,fec->GetBaseExtSeq()-minSeq);
			DumpBits(mediaMask);
			
			//Compare needed pacekts with actual pacekts, to check if we have all of them except the missing
			if ((fecMask & mediaMask) == fecMask)
			{
				Debug("----------recovered!!");
				//Rocovered media data
				BYTE	recovered[MTU];
				//Get attributes
				bool  p  = fec->GetRecoveryP();
				bool  x  = fec->GetRecoveryX();
				BYTE  cc = fec->GetRecoveryCC();
				bool  m  = fec->GetRecoveryM();
				BYTE  pt = fec->GetRecoveryType();
				DWORD ts = fec->GetRecoveryTimestamp();
				WORD  l  = fec->GetRecoveryLength();
				//Get protection length
				DWORD level0Size = fec->GetLevel0Size();
				//Copy data
				memcpy(recovered,fec->GetLevel0Data(),level0Size);
				//Set value in temp buffer
				set8(aux,0,fecMask);
				//Get bit reader
				BitReader r(aux,8);
				//Read all media packet
				while(r.Left())
				{
					bool x = r.Get(1);
					Debug("%d",x);
					//If the media packet is used to reconstrud the packet
					if (x)
					{
						Debug("-recovering from %d %d\n",fec->GetBaseExtSeq()+r.GetPos()-1,r.GetPos()-1);
						//Get media packet
						RTPTimedPacket* media = medias[fec->GetBaseExtSeq()+r.GetPos()-1];
						//Retreive
						Debug("%u\n",media->GetTimestamp());
						//Calculate receovered attributes
						p  ^= media->GetP();
						x  ^= media->GetX();
						cc ^= media->GetCC();
						m  ^= media->GetMark();
						pt ^= media->GetType();
						ts ^= media->GetTimestamp();
						l  ^= media->GetMediaLength();
						//Get data
						BYTE *payload = media->GetMediaData();
						//Calculate the xor
						for (int i=0;i<fmin(media->GetMediaLength(),level0Size);++i)
							//XOR
							recovered[i] ^= payload[i];
					}
				}
				//Create new video packet
				RTPTimedPacket* packet = new RTPTimedPacket(MediaFrame::Video,pt);
				//Set values
				packet->SetP(p);
				packet->SetX(x);
				packet->SetMark(m);
				packet->SetTimestamp(ts);
				//Set sequence number
				packet->SetSeqNum(seq);
				//Set seq cycles
				packet->SetSeqCycles(fec->GetBaseSeqCylcles());
				//Set ssrc
				packet->SetSSRC(ssrc);
				//Set payload and recovered length
				packet->SetPayload(recovered,l);
				Debug("-recovered packet len:%u ts:%u pts:%u seq:%d\n",l,ts,packet->GetTimestamp() ,packet->GetSeqNum());
				Dump(packet->GetData(),32);

				//Append the packet to the media packet list
				AddPacket(packet);
				//Return it
				return packet;
			}
		}
	}
	//Nothing found
	return NULL;
}