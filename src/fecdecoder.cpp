/*
 * File:   fecdecoder.cpp
 * Author: Sergio
 *
 * Created on 6 de febrero de 2013, 10:30
 */

#include <set>
#include <srtp2/srtp.h>

#include "fecdecoder.h"
#include "codecs.h"
#include "bitstream.h"


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

bool FECDecoder::AddPacket(RTPPacket* packet)
{
	//Check if it is a redundant packet
	if (packet->GetCodec()==VideoCodec::RED)
	{
		//Get the redundant packet
		RTPRedundantPacket *red = (RTPRedundantPacket *)packet;

		//Check primary redundant type
		if (red->GetPrimaryCodec()==VideoCodec::ULPFEC)
		{
			//Create new FEC data
			FECData *fec = new FECData(red->GetPrimaryPayloadData(),red->GetPrimaryPayloadSize());
			//Log
			Debug("-fec primary red data at %d\n",fec->GetBaseExtSeq());
			//Append it
			codes.insert(std::pair<DWORD,FECData*>(fec->GetBaseExtSeq(),fec));
			//Packet contained no media
			return false;
		}

		//Ensure we don't have it already
		if (medias.find(packet->GetExtSeqNum())==medias.end())
			//Add media packet
			medias[packet->GetExtSeqNum()] = red->CreatePrimaryPacket();

		//For each redundant data
		for (int i=0;i<red->GetRedundantCount();++i)
		{
			//Check if it is a FEC pacekt
			if (red->GetRedundantCodec(i)==VideoCodec::ULPFEC)
			{
				//Create new FEC data
				FECData *fec = new FECData(red->GetRedundantPayloadData(i),red->GetRedundantPayloadSize(i));
				//Log
				Debug("-fec red data at %d\n",fec->GetBaseExtSeq());
				//Append it
				codes.insert(std::pair<DWORD,FECData*>(fec->GetBaseExtSeq(),fec));
			}
		}
	} else if (packet->GetCodec()==VideoCodec::ULPFEC ) {
		//Create new FEC data
		FECData *fec = new FECData(packet->GetMediaData(),packet->GetMediaLength());
		//Log
		Debug("-fec data at %d\n",fec->GetBaseExtSeq());
		//Append it
		codes.insert(std::pair<DWORD,FECData*>(fec->GetBaseExtSeq(),fec));
		//Packet contained no media
		return false;
	} else if  (packet->GetCodec()==VideoCodec::FLEXFEC) {
		//TODO
		return false;
	} else {
		//Check if we have it already
		if (medias.find(packet->GetExtSeqNum())!=medias.end())
			//Do nothing
			return false;
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

	//Packet had media
	return true;
}

RTPPacket* FECDecoder::Recover()
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
	memset(aux,0,8);
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

		//Search FEC packets associated this media packet
		for (FECOrderedData::iterator it2 = codes.begin();it2!=codes.end();++it2)
		{
			//Get FEC packet
			FECData *fec = it2->second;

			//Check if it is associated with this media pacekt in level 0
			if (!fec->IsProtectedAtLevel0(seq))
				//Next
				continue;

			//Get the seq difference between fec data and the media
			// fec seq has to be <= media seq it fec data protect media data)
			DWORD diff = seq-fec->GetBaseExtSeq();
			//Shit mask of the lost packets to check the present ones from the base seq
			QWORD mediaMask = lostMask << (fec->GetBaseExtSeq()-minSeq);
			//Remove lost packet bit from the fec mask
			QWORD fecMask = fec->GetLevel0Mask() & ~(((QWORD)1)<<(64-diff-1));

			//Compare needed pacekts with actual pacekts, to check if we have all of them except the missing
			if ((fecMask & mediaMask) == fecMask)
			{
				//Rocovered media data
				BYTE	recovered[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
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
				//Ensure there is enought size
				if (level0Size>MTU)
				{
					//Error
					Error("-FEC level 0 data size too big [%d]\n",level0Size);
					//Skip this one
					continue;
				}
				//Copy data
				memcpy(recovered,fec->GetLevel0Data(),level0Size);
				//Set value in temp buffer
				set8(aux,0,fecMask);
				//Get bit reader
				BitReader r(aux,8);
				//Read all media packet
				while(r.Left())
				{
					//If the media packet is used to reconstrud the packet
					if (r.Get(1))
					{
						//Get media packet
						RTPPacket* media = medias[fec->GetBaseExtSeq()+r.GetPos()-1];
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
				RTPPacket* packet = new RTPPacket(MediaFrame::Video,pt);
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
				if (!packet->SetPayloadWithExtensionData(recovered,l))
				{
					//Delete packet
					delete(packet);
					//Error
					Error("-FEC payload of recovered packet to big [%u]\n",(unsigned int)l);
					//Skip
					continue;
				}

				Debug("-recovered packet len:%u ts:%u pts:%u seq:%d\n",l,ts,packet->GetTimestamp() ,packet->GetSeqNum());

				//Append the packet to the media packet list
				if (AddPacket(packet))
					//Return it if contained media
					return packet;
				else
					//Discard and continue
					delete(packet);
			}
		}
	}
	//Nothing found
	return NULL;
}
