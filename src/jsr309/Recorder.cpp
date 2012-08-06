/* 
 * File:   Recorder.cpp
 * Author: Sergio
 * 
 * Created on 26 de febrero de 2012, 16:50
 */

#include "Recorder.h"
#include "log.h"

Recorder::Recorder(std::wstring tag)
{
	//Store tag
	this->tag = tag;
	//NO audio or video
	audio = NULL;
	video = NULL;
}

Recorder::~Recorder()
{
	//If we have an audio depacketizer
	if (audio)
		//Delete it
		delete(audio);
	//If we have an video depacketizer
	if (video)
		//Delete it
		delete(video);
}

void Recorder::onRTPPacket(RTPPacket &packet)
{
	//Check type
	switch(packet.GetMedia())
	{
		case MediaFrame::Audio:
			break;
		case MediaFrame::Video:
			//Do we have video depacketizer
			if (!video)
				//Create new
				video = RTPDepacketizer::Create(packet.GetMedia(),packet.GetCodec());
			//Check again
			if (video)
			{
				//Append to frame
				VideoFrame *frame = (VideoFrame*)video->AddPacket(&packet);
				//Is it last
				if (packet.GetMark())
				{
					//If got frame
					if (frame)
						//Record frame
						onMediaFrame(*frame);
					//Clear frame
					video->ResetFrame();
				}
			}
			break;
		case MediaFrame::Text:
			break;
	}

}

void Recorder::onResetStream()
{
	//Do nothing by now
}

void Recorder::onEndStream()
{
	//Do nothing by now
}

//Attach
int Recorder::Attach(MediaFrame::Type media, Joinable *join)
{
	Log("-Endpoint attaching [media:%d]\n",media);

	//Get joined
	JoinedMap::iterator it = joined.find(media);

	//Detach if joined
	if (it!=joined.end())
	{
		//Remove ourself as listeners
		it->second->RemoveListener(this);
		//Remove from map
		joined.erase(it);
	}

	//If it is not null
	if (join)
	{
		//Set in map
		joined[media] = join,
		//Join to the new one
		join->AddListener(this);
	}

	return 1;
}

int Recorder::Dettach(MediaFrame::Type media)
{
	Log("-Endpoint detaching [media:%d]\n",media);

	//Get joined
	JoinedMap::iterator it = joined.find(media);

	//Detach if joined
	if (it!=joined.end())
	{
		//Remove ourself as listeners
		it->second->RemoveListener(this);
		//Remove from map
		joined.erase(it);
	}

	return 1;
}
