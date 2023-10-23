#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>



#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "rtmp/rtmpmp4stream.h"
#include "rtmp/flvrecorder.h"
#include "audio.h"
#include "AudioCodecFactory.h"

#ifdef FLV1PARSER
#include "flv1/flv1Parser.h"
#endif
	
RTMPMP4Stream::RTMPMP4Stream(DWORD id,RTMPNetStream::Listener *listener) : RTMPNetStream(id,listener), streamer(this)
{
	encoder = NULL;
	decoder = NULL;
	desc = NULL;
}

RTMPMP4Stream::~RTMPMP4Stream()
{
	if (encoder)
		delete(encoder);
	if (decoder)
		delete(decoder);
	if (desc)
		delete(desc);
	//Just in case
	streamer.Close();
}

void RTMPMP4Stream::doPlay(QWORD transId, std::wstring& url,RTMPMediaStream::Listener* listener)
{
	char filename[1024];

	//Print it
	snprintf(filename,1024,"%ls",url.c_str());

	//Open it and play
	if (!streamer.Open(filename))
	{
		//Send error comand
		fireOnNetStreamStatus(transId, RTMP::Netstream::Play::StreamNotFound,L"Stream not found");
		//Error
		Error("Error opening mp4 file [path:\"%ls\"",filename);
		//Exit
		return;
	}

	//Add listener
	AddMediaListener(listener);

	//Send play comand
	fireOnNetStreamStatus(transId, RTMP::Netstream::Play::Reset,L"Playback reset");

	//Send play comand
	fireOnNetStreamStatus(transId, RTMP::Netstream::Play::Start,L"Playback started");

	//Create metadata object
	RTMPMetaData *meta = new RTMPMetaData(0);

	//Set name
	meta->AddParam(new AMFString(L"onMetaData"));

	//Create properties string
	AMFEcmaArray *prop = new AMFEcmaArray();

	//Add default properties
	if (streamer.HasAudioTrack())
	{
		switch (streamer.GetAudioCodec())
		{
			case AudioCodec::PCMU:
				//Set properties
				prop->AddProperty(L"audiocodecid"	,(float)RTMPAudioFrame::SPEEX	);	//Number Audio codec ID used in the file (see E.4.2.1 for available SoundFormat values)
				prop->AddProperty(L"audiodatarate"	,(float)16000			);	// Number Audio bit rate in kilobits per second
				//Set decoder
				decoder = AudioCodecFactory::CreateDecoder(AudioCodec::PCMU);
				//Set encoder to speek
				encoder = AudioCodecFactory::CreateEncoder((AudioCodec::SPEEX16));
				break;
			case AudioCodec::PCMA:
				prop->AddProperty(L"audiocodecid"	,(float)RTMPAudioFrame::SPEEX	);	//Number Audio codec ID used in the file (see E.4.2.1 for available SoundFormat values)
				prop->AddProperty(L"audiodatarate"	,(float)16000			);	// Number Audio bit rate in kilobits per second
				//Set decoder
				decoder =  AudioCodecFactory::CreateDecoder(AudioCodec::PCMA);
				//Setencoder to speek
				encoder = AudioCodecFactory::CreateEncoder(AudioCodec::SPEEX16);
				break;
		}
		//prop->AddProperty(L"stereo"		,new AMFBoolean(false)	);	// Boolean Indicating stereo audio
		prop->AddProperty(L"audiodelay"		,0.0	);	// Number Delay introduced by the audio codec in seconds
		prop->AddProperty(L"audiosamplerate"	,8000.0	);	// Number Frequency at which the audio stream is replayed
		prop->AddProperty(L"audiosamplesize"	,160.0	);	// Number Resolution of a single audio sample
	}

	//If ti has video track
	if (streamer.HasVideoTrack())
	{
		switch (streamer.GetVideoCodec())
		{
			case VideoCodec::H263_1996:
			case VideoCodec::H263_1998:
				prop->AddProperty(L"videocodecid"	,(float)RTMPVideoFrame::FLV1	);	// Number Video codec ID used in the file (see E.4.3.1 for available CodecID values)
				break;
			case VideoCodec::H264:
				prop->AddProperty(L"videocodecid"	,new AMFString(L"avc1")		);	// Number Video codec ID used in the file (see E.4.3.1 for available CodecID values)
				break;
		}

		prop->AddProperty(L"framerate"		,(float)streamer.GetVideoFramerate()	);	// Number Number of frames per second
		prop->AddProperty(L"height"		,(float)streamer.GetVideoHeight()	);	// Number Height of the video in pixels
		prop->AddProperty(L"videodatarate"	,(float)streamer.GetVideoBitrate()/1024	);	// Number Video bit rate in kilobits per second
		prop->AddProperty(L"width"		,(float)streamer.GetVideoWidth()	);	// Number Width of the video in pixels
	}

	prop->AddProperty(L"canSeekToEnd"	,0.0	);			// Boolean Indicating the last video frame is a key frame
	prop->AddProperty(L"duration"		,(float)streamer.GetDuration());	// Number Total duration of the file in seconds

	//Add param
	meta->AddParam(prop);

	//Send metadata
	SendMetaData(meta);

	//Get AVC descriptor if any
	desc = streamer.GetAVCDescriptor().release();

	//If we have one
	if (desc)
	{
		//Create the frame
		RTMPVideoFrame fdesc(0,*desc);
		//Play it
		SendMediaFrame(&fdesc);
	}

	//Play it
	if (!streamer.Play())
	{
		//Close it
		streamer.Close();
		//Send error comand
		fireOnNetStreamStatus(transId, RTMP::Netstream::Play::Failed,L"Error openeing stream");
		//Error
		Error("Error starting playback of mp4 file [path:\"%ls\"",url.c_str());
	}

}

void RTMPMP4Stream::doSeek(QWORD transId, DWORD time)
{
	//Get
	QWORD seeked = streamer.PreSeek(time);

	//If done
	if (seeked!=-1)
	{
		//Stop playback
		streamer.Stop();
		//Reset connection
		Reset();
		//Send status update
		SendCommand(L"onStatus", new RTMPNetStatusEvent(L"NetStream.Seek.Notify",L"status",L"Seek done"));
		//Send play comand
		SendCommand(L"onStatus", new RTMPNetStatusEvent(L"NetStream.Play.Start",L"status",L"Playback started") );

		//If we have one
		if (desc)
		{
			//Create the frame
			RTMPVideoFrame fdesc(seeked,*desc);
			//Play it
			SendMediaFrame(&fdesc);
		}
		//Seek
		streamer.Seek(seeked);
	} else
		//Send status update
		SendCommand(L"onStatus", new RTMPNetStatusEvent(L"NetStream.Seek.Failed",L"status",L"Seek failed"));

}

void RTMPMP4Stream::doClose(QWORD transId, RTMPMediaStream::Listener* listener)
{
	//Close streamer
	streamer.Close();
	
	//Remover listener
	RemoveMediaListener(listener);
}

void RTMPMP4Stream::onRTPPacket(RTPPacket &packet)
{
	//Do nothing
	return;
}
void RTMPMP4Stream::onTextFrame(TextFrame &text)
{
	AMFObject *obj = new AMFObject();
	//Add text
	obj->AddProperty(L"text",text.GetWChar(),text.GetWLength());
	//Send packet
	SendCommand(L"onTextData",obj);
}

void RTMPMP4Stream::onMediaFrame(MediaFrame &media)
{
	//Get timestamp in 1000ms clock rate
	QWORD timestamp = media.GetTimeStamp()*1000/media.GetClockRate();
	
	//Depending on the media type
	switch (media.GetType())
	{
		case MediaFrame::Audio:
		{
			//Create rtmp frame
			RTMPAudioFrame *frame = new RTMPAudioFrame(timestamp,512);
			//Get audio frame
			AudioFrame& audio = (AudioFrame&)media;
			//Check codec
			switch(audio.GetCodec())
			{
				case AudioCodec::PCMA:
				case AudioCodec::PCMU:
				{
					SWORD raw[512];
					DWORD rawsize = 512;
					//Decode audio frame
					DWORD rawlen = decoder->Decode(audio.GetData(),audio.GetLength(),raw,rawsize);
					//Encode frame
					DWORD len = encoder->Encode(raw,rawlen,frame->GetMediaData(),frame->GetMaxMediaSize());
					//Set length
					frame->SetMediaSize(len);
					//Set type
					frame->SetAudioCodec(RTMPAudioFrame::SPEEX);
					frame->SetSoundRate(RTMPAudioFrame::RATE11khz);
					frame->SetSamples16Bits(1);
					frame->SetStereo(0);
					break;
				}
				default:
					//Not supported
					return;
			}
			//Send it
			SendMediaFrame(frame);
		}
		break;
		case MediaFrame::Video:
		{
			//Get video frame
			VideoFrame& video = (VideoFrame&)media;
			//Create rtmp frame
			RTMPVideoFrame *frame = new RTMPVideoFrame(timestamp,video.GetLength());
			//Check codec
			switch(video.GetCodec())
			{
				case VideoCodec::H263_1996:
				case VideoCodec::H263_1998:
				#ifdef FLV1PARSER
				{
					//Create FLV1parser in case we need it
					flv1Parser *parser = new flv1Parser(frame->GetMediaData(),frame->GetMaxMediaSize());
					//Proccess
					if (!parser->FrameFromH263(video.GetData(),video.GetLength()))
						throw new std::exception();
					//Set lengtht
					frame->SetMediaSize(parser->GetSize());
					//If it is intra
					if (video.IsIntra())
						//Set type
						frame->SetFrameType(RTMPVideoFrame::INTRA);
					else
						//Set type
						frame->SetFrameType(RTMPVideoFrame::INTER);
					//Set type
					frame->SetVideoCodec(RTMPVideoFrame::FLV1);
				}
				#endif
					break;
				case VideoCodec::H264:
				{
					//Set Codec
					frame->SetVideoCodec(RTMPVideoFrame::AVC);
					//If it is intra
					if (video.IsIntra())
					{
						//Set type
						frame->SetFrameType(RTMPVideoFrame::INTRA);
						//If we have one
						if (desc)
						{
							//Create the fraame
							RTMPVideoFrame fdesc(timestamp,*desc);
							//Play it
							SendMediaFrame(&fdesc);
						}
					} else {
						//Set type
						frame->SetFrameType(RTMPVideoFrame::INTER);
					}
					//Set NALU type
					frame->SetAVCType(RTMPVideoFrame::AVCNALU);
					//Set no delay
					frame->SetAVCTS(0);
					//Set Data
					frame->SetVideoFrame(video.GetData(),video.GetLength());
					break;
				}
				default:
					//Not supported
					return;
			}
			//Send it
			SendMediaFrame(frame);
			//Delete it
			delete(frame);
		}
		break;
	}
}

void RTMPMP4Stream::onEnd()
{
	//Send command
	SendStreamEnd();
}
