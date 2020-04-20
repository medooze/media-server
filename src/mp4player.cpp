#include "mp4player.h"
#include "log.h"
#include "codecs.h"
#include "AudioCodecFactory.h"
#include "VideoCodecFactory.h"

MP4Player::MP4Player() : streamer(this)
{
	audioDecoder = NULL;
	videoDecoder = NULL;
}

MP4Player::~MP4Player()
{
	//End
	End();
}

int MP4Player::Init(AudioOutput *audioOutput,VideoOutput *videoOutput,TextOutput *textOutput)
{
	//Sava
	this->audioOutput = audioOutput;
	this->videoOutput = videoOutput;
	this->textOutput = textOutput;
}

int MP4Player::Play(const char* filename,bool loop)
{
	Log("-MP4Player play [\"%s\"]\n",filename);

	//Stop just in case
	streamer.Close();

	//Open file
	if (!streamer.Open(filename))
		//Error
		return Error("Error opening mp4 file");

	//Save loop value
	this->loop = loop;

	//Open audio codec
	if (streamer.HasAudioTrack())
		//Create audio codec
		audioDecoder = AudioCodecFactory::CreateDecoder((AudioCodec::Type)streamer.GetAudioCodec());

	//Open video codec
	if (streamer.HasVideoTrack())
		//Create audio codec
		videoDecoder = VideoCodecFactory::CreateDecoder((VideoCodec::Type)streamer.GetVideoCodec());
		
	//Start playback
	return streamer.Play();
}

int MP4Player::Stop()
{
	Log("-MP4Player stop\n");

	//Do not loop anymore
	loop = false;

	//Stop
	streamer.Stop();
	
	//Close
	streamer.Close();

	//Delete codecs
	if (audioDecoder)
	{
		delete (audioDecoder);
		audioDecoder = NULL;
	}
	if (videoDecoder)
	{
		delete (videoDecoder);
		videoDecoder = NULL;
	}
	
	return 1;
}

int MP4Player::End()
{
	Stop();
}

void MP4Player::onTextFrame(TextFrame &text)
{
	Log("-On TextFrame [\"%ls\"]\n",text.GetWChar());

	//Check textOutput
	if (textOutput)
		//Publish it
		textOutput->SendFrame(text);
}

void MP4Player::onRTPPacket(RTPPacket &packet)
{
	SWORD buffer[1024];
	DWORD bufferSize = 1024;
	//Get data
	const BYTE *data = packet.GetMediaData();
	//Get leght
	DWORD len = packet.GetMediaLength();
	//Get mark
	bool mark = packet.GetMark();
	
	//Depending on the media
	switch (packet.GetMediaType())
	{
		case MediaFrame::Audio:
			//Check decoder
			if (!audioDecoder || !audioOutput)
				//Do nothing
				return;

			//Decode it
			len = audioDecoder->Decode(data,len,buffer,bufferSize);

			//Play it
			audioOutput->PlayBuffer(buffer,len,0);

			break;
		case MediaFrame::Video:
			//Check decoder
			if (!videoDecoder || !videoOutput)
				//Do nothing
				return;
			
			//Decode packet
			if(!videoDecoder->DecodePacket(data,len,false,mark))
				//Error
				return;
			//Check if it is last
			if(mark)
			{
				//Get dimensions
				DWORD width = videoDecoder->GetWidth();
				DWORD height= videoDecoder->GetHeight();
				//Set it
				videoOutput->SetVideoSize(width,height);
				//Set decoded frame
				videoOutput->NextFrame(videoDecoder->GetFrame());
			}
			break;
	}

}

void MP4Player::onMediaFrame(const MediaFrame &frame)
{
	//Do nothing now
}
void MP4Player::onMediaFrame(DWORD ssrc, const MediaFrame &frame)
{
	//Do nothing now
}

void MP4Player::onEnd()
{
	//If in loop
	if (loop)
		//Play again
		streamer.Play();
}
