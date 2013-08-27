#include "tools.h"
#include "rtmpmessage.h"
#include "avcdescriptor.h"
#include <stdexcept>
#include <cstdlib>

/************************************
 * RTMPMesssage
 *
 ***********************************/

RTMPMessage::RTMPMessage(DWORD streamId,QWORD timestamp,Type type,DWORD length) 
{
	//Store values
	this->streamId 	= streamId;
	this->type 	= type;
	this->length 	= length;
	this->timestamp = timestamp;

	//Nothing
	cmd 	= NULL;
	ctrl 	= NULL;
	meta 	= NULL;
	media 	= NULL;

	//No skip
	skip = false;

	//Depending on the type
	switch(type)
	{
		case SetChunkSize:
			//Create ctrl msg
			ctrl = new RTMPSetChunkSize();
			break;
                case AbortMessage:
			//Create ctrl msg
			ctrl = new RTMPAbortMessage();
			break;
                case Acknowledgement:
			//Create ctrl msg
			ctrl = new RTMPAcknowledgement();
			break;
                case UserControlMessage:
			//Create ctrl msg
			ctrl = new RTMPUserControlMessage();
			break;
                case WindowAcknowledgementSize:
			//Create ctrl msg
			ctrl = new RTMPWindowAcknowledgementSize();
			break;
                case SetPeerBandwidth:
			//Create ctrl msg
			ctrl = new RTMPSetPeerBandWidth();
			break;
		case CommandAMF3:
			//Skip first byte if cero
			skip = true;
			//Create cmd message
			cmd = new RTMPCommandMessage();
			break;
		case Command:
			//Create cmd message
			cmd = new RTMPCommandMessage();
			break;
		case DataAMF3:
			//Skip first byte if cero
			skip = true;
			//Reset amf parser
			meta = new RTMPMetaData(timestamp);
			break;
		case Data:
			//Reset amf parser
			meta = new RTMPMetaData(timestamp);
			break;
		case SharedObjectAMF3:
		case RTMPMessage::SharedObject:
			//TODO: parse shared object
			break;
		case Audio:
			//Check length
			if (length)
				//Create frame
				media = new RTMPAudioFrame(timestamp,length);
			break;
		case Video:
			//Check length
			if (length)
				//Create frame
				media = new RTMPVideoFrame(timestamp,length);
			break;
		default:
			std::runtime_error("RTMP Message not supported");
	}

	//Init position
	pos = 0;
	//We are parsing again
	parsing = true;
}

RTMPMessage::RTMPMessage(DWORD streamId,QWORD timestamp,Type type,RTMPObject* ctrl) 
{
	//Store values
	this->streamId = streamId;
	this->type = type;
	this->timestamp = timestamp;
	//Calculate message size
	this->length = ctrl->GetSize();
	//Store msg
	this->ctrl = ctrl;
	this->cmd = NULL;
	this->meta = NULL;
	this->media = NULL;
}

RTMPMessage::RTMPMessage(DWORD streamId,QWORD timestamp,RTMPCommandMessage* cmd)
{
	//Store values
	this->streamId = streamId;
	this->type = RTMPMessage::Command;
	this->timestamp = timestamp;
	//Calculate message size
	this->length = cmd->GetSize();
	//Store msg
	this->ctrl = NULL;
	this->cmd = cmd;
	this->meta = NULL;
	this->media = NULL;
}

RTMPMessage::RTMPMessage(DWORD streamId,QWORD timestamp,RTMPMediaFrame* media)
{
	//Store values
	this->streamId = streamId;
	
	switch(media->GetType())
	{
		case RTMPMediaFrame::Audio:
			this->type = RTMPMessage::Audio;
			break;
		case RTMPMediaFrame::Video:
			this->type = RTMPMessage::Video;
			break;
	}
	this->timestamp = timestamp;
	//Calculate message size
	this->length = media->GetSize();
	//Store msg
	this->ctrl = NULL;
	this->cmd = NULL;
	this->meta = NULL;
	this->media = media;
}

RTMPMessage::RTMPMessage(DWORD streamId,QWORD timestamp,RTMPMetaData* meta)
{
	//Store values
	this->streamId = streamId;
	this->type = RTMPMessage::Data;
	this->timestamp = timestamp;
	//Calculate message size
	this->length = meta->GetSize();
	//Store msg
	this->ctrl = NULL;
	this->cmd = NULL;
	this->meta = meta;
	this->media = NULL;
}

RTMPMessage::~RTMPMessage()
{
	//Free
	if (ctrl)	
		delete(ctrl);
	if (cmd)
		delete(cmd);
	if (meta)
		delete(meta);
	if (media)
		delete(media);
}

DWORD RTMPMessage::Serialize(BYTE* data,DWORD size)
{
	if (ctrl)
		return ctrl->Serialize(data,size);
	if (cmd)
		return cmd->Serialize(data,size);
	if (meta)
		return meta->Serialize(data,size);
	if (media)
		return media->Serialize(data,size);
	return 0;
}

void RTMPMessage::Dump()
{
	Debug("[RTMPMessage type:%s,streamId:%u,length:%u,timestamp:%lu]\n",TypeToString(type),streamId,length,timestamp);
	if (ctrl)
	{
		//Check type
		switch((RTMPMessage::Type)type)
		{
			case RTMPMessage::SetChunkSize:
				//Get new chunk size
				Debug("\tSetChunkSize [size:%d]\n",((RTMPSetChunkSize *)ctrl)->GetChunkSize());
				break;
			case RTMPMessage:: AbortMessage:
				Debug("\tAbortMessage [chunkId:%d]\n",((RTMPAbortMessage*)ctrl)->GetChunkStreamId());
				break;
			case RTMPMessage::Acknowledgement:
				Debug("\tAcknowledgement [seq:%d]\n",((RTMPAcknowledgement*)ctrl)->GetSeNumber());
				break;
			case RTMPMessage::UserControlMessage:
			{
				//Get event
				RTMPUserControlMessage* event = (RTMPUserControlMessage*)ctrl;
				Debug("\tUserControl [%d] ",event->GetEventType());
				//Depending on the event received
				switch(event->GetEventType())
				{
					case RTMPUserControlMessage::StreamBegin:
						Debug("StreamBegin [stream:%d]\n",event->GetEventData());
						break;
					case RTMPUserControlMessage::StreamEOF:
						Debug("StreamEOF [stream:%d]\n",event->GetEventData());
						break;
					case RTMPUserControlMessage::StreamDry:
						Debug("StreamDry [stream:%d]\n",event->GetEventData());
						break;
					case RTMPUserControlMessage::SetBufferLength:
						Debug("SetBufferLength [stream:%d,size:%d]\n",event->GetEventData(),event->GetEventData2());
						break;
					case RTMPUserControlMessage::StreamIsRecorded:
						Debug("StreamIsRecorded [stream:%d]\n",event->GetEventData());
						break;
					case RTMPUserControlMessage::PingRequest:
						Debug("PingRequest [milis:%d]\n",event->GetEventData());
						break;
					case RTMPUserControlMessage::PingResponse:
						Debug("PingResponse [milis:%d]\n",event->GetEventData());
						break;

				}
			}
				break;
			case RTMPMessage::WindowAcknowledgementSize:
				Debug("\tWindowAcknowledgementSize [size:%d]\n",((RTMPWindowAcknowledgementSize*)ctrl)->GetWindowSize());
				break;
			case RTMPMessage::SetPeerBandwidth:
				Debug("\tSetPeerBandwidth [size:%d]\n",((RTMPSetPeerBandWidth*)ctrl)->GetWindowSize());
				break;
		}
	}
	if (cmd)
		 cmd->Dump();
	if (meta)
		 meta->Dump();
	if (media)
		 media->Dump();
	Debug("[/RTMPMessage]\n");
}

DWORD RTMPMessage::Parse(BYTE* data,DWORD size)
{
	//Get pointer and data size
	DWORD len = size;

	//Get reamining data
	DWORD left = length - pos;

	//Check sizes
	if (len > left)
		//Limit size
		len = left;

	if (IsControlProtocolMessage())
	{
		//Check if we have a ctrl object and parse
		if (ctrl)
		{
			//Parse
			pos += ctrl->Parse(data,len);
			//Check if we have finished
			parsing = !ctrl->IsParsed();
		}
	} else if (IsCommandMessage()) {
		//Check if we have a msg and parse
		if (cmd)
		{
			//If it is an amf3 object skip the first byte if 0
			if (skip && data[0]==0)
			{
				//Parse
				pos +=  cmd->Parse(data+1,len-1)+1;
				//Not skip more
				skip = false;
			} else 
				//Parse
				pos +=  cmd->Parse(data,len);
		}
		//Check if we have finished
		parsing = (pos!=length);
	} else if (IsMedia()) {
		//Check we have buffer allocated
		if (media)
			//Parse the media
			pos += media->Parse(data,len);
		//Check if we have finished
		parsing = (pos!=length);
	} else if (IsMetaData()) {
		if (meta)
		{
			//If it is an amf3 object skip the first byte if 0
			if (skip && data[0]==0)
			{
				//Parse
				pos += meta->Parse(data+1,len-1)+1;
				//Not skip more
				skip = false;
			} else 
				//Parse
				pos += meta->Parse(data,len);
			
		}
		//Check if we have finished
		parsing = (pos!=length);
	} else if (IsSharedObject()) {
		//Skip 
		pos +=len;
		//Check if we have finished
		parsing = (pos!=length);
	}

	return len;
}

bool RTMPMessage::IsParsed()
{
	return !parsing;
}

bool RTMPMessage::IsControlProtocolMessage()
{
	//Depending on the type
	switch(type)
	{
		case RTMPMessage::SetChunkSize:
                case RTMPMessage::AbortMessage:
                case RTMPMessage::Acknowledgement:
                case RTMPMessage::UserControlMessage:
                case RTMPMessage::WindowAcknowledgementSize:
                case RTMPMessage::SetPeerBandwidth:
			return true;
	}
	return false;
}

bool RTMPMessage::IsCommandMessage()
{
	//Depending on the type
	switch(type)
	{
		case RTMPMessage::Command:
		case RTMPMessage::CommandAMF3:
			return true;
	}
	return false;
}

bool RTMPMessage::IsMetaData()
{
	//Depending on the type
	switch(type)
	{
		case RTMPMessage::Data:
		case RTMPMessage::DataAMF3:
			return true;
	}
	return false;
}

bool RTMPMessage::IsSharedObject()
{
	//Depending on the type
	switch(type)
	{
		case RTMPMessage::SharedObject:
		case RTMPMessage::SharedObjectAMF3:
			return true;
	}
	return false;
}

bool RTMPMessage::IsMedia()
{
	//Depending on the type
	switch(type)
	{
		case RTMPMessage::Audio:
		case RTMPMessage::Video:
			return true;
	}
	return false;
}


/********************************
 * RTMPCommandMessage
 *
 *********************************/
RTMPCommandMessage::RTMPCommandMessage()
{
	//Set data to null
	name = NULL;
	transId = NULL;
	params = NULL;
}

RTMPCommandMessage::RTMPCommandMessage(const wchar_t* name,QWORD transId,AMFData* params,AMFData* extra)
{
	//Set name
	this->name = new AMFString();
	this->name->SetWString(name);
	//Set transId
	this->transId = new AMFNumber();
	this->transId->SetNumber(transId);
	//Store objects
	if (params)
	    //Store object
	    this->params = params;
	else
	    //Create null object
	    this->params = new AMFNull();
	//If extra
	if (extra)
		this->extra.push_back(extra);
}


RTMPCommandMessage::~RTMPCommandMessage()
{
	//If parsed objects
	if (name)
		delete(name);	
	if (transId)
		delete(transId);	
	if (params)
		delete(params);	
	//For each extra param
	for(DWORD i=0;i<extra.size();i++)
		//Delete object
		delete(extra[i]);
}

DWORD RTMPCommandMessage::Parse(BYTE *data,DWORD size)
{
	BYTE* buffer = data;
	DWORD bufferSize = size;

	while(bufferSize)
	{
		//Parse amf object
		DWORD len = parser.Parse(buffer,bufferSize);

		//Remove used data
		buffer += len;
		bufferSize -= len;

		//Check if we got another object
		if (parser.IsParsed())
		{
			//Get Object
			AMFData* obj = parser.GetObject();
			//Dependind on the parsing state
			if (!name)
			{	
				//Assert it is a string
				obj->AssertType(AMFData::String);
				//Set cmd name
				name = (AMFString*)obj;
			} else if (!transId) {
				//Assert it is a number
				obj->AssertType(AMFData::Number);
				//Set transaction id
				transId = (AMFNumber*)obj;
			} else if (!params) {
				//Set param object
				params = obj;
			} else {
				//Add extra objects
				extra.push_back(obj);
			}
		}
	}

	return (buffer-data);
}

void RTMPCommandMessage::Dump()
{
	AMFNull null;

	Debug("[RTMPCommandMessage name:%ls transId:%d]\n",name->GetWString().c_str(),(DWORD)transId->GetNumber());
	if (params)
		params->Dump();
	else
		null.Dump();
	for(DWORD i=0;i<extra.size();i++)
	{
		//Get object
		AMFData* obj = extra[i];
		//Check param
		if (obj)
			obj->Dump();
		else
			null.Dump();
	}
	Debug("[/RTMPCommandMessage]\n");
}


DWORD RTMPCommandMessage::Serialize(BYTE* buffer,DWORD size)
{
	AMFNull null;

	//Calculate size
	DWORD len = GetSize();

	//Check if we have enought size
	if (size<len)
		//Throw error
		return -1;
	
	//Reset
	len = 0;

	//Check if we have name
	if (name)
		//Serialize name
		len += name->Serialize(buffer+len,size-len);
	else
		len += null.Serialize(buffer+len,size-len);
	//Check if we have trans id
	if (transId)
		//Serialize trans id
		len += transId->Serialize(buffer+len,size-len);
	else	
		len += null.Serialize(buffer+len,size-len);
	//Check if we have params
	if (params)
		//Serialize params
		len += params->Serialize(buffer+len,size-len);
	else
		len += null.Serialize(buffer+len,size-len);
	
	//Check if we have extr;	
	for(DWORD i=0;i<extra.size();i++)
	{
		//Get object
		AMFData* obj = extra[i];
		//Check param
		if (obj)
			//Serialize extra
			len += obj->Serialize(buffer+len,size-len);
		else
			//Serialize null
			len += null.Serialize(buffer+len,size-len);
	}

	return len;	

}

DWORD RTMPCommandMessage::GetSize()
{
	AMFNull null;

	//Get base length
	DWORD len = 0;

	//Check if we have name
	if (name)
		len += name->GetSize();
	else
		len += null.GetSize();
	//Check if we have trans id
	if (transId)
		len += transId->GetSize();
	else
		len += null.GetSize();

	//Check if we have params
	if (params)
		len += params->GetSize();
	else
		len += null.GetSize();

	//Check if we have extr;	
	for(DWORD i=0;i<extra.size();i++)
	{
		//Get object
		AMFData* obj = extra[i];
		//Check param
		if (obj)
			//Get extra size
			len += obj->GetSize();
		else
			//Add null
			len += null.GetSize();
	}

	//Return lengt
	return len;
}
/************************
 * RTMPMetaData
 *
 *************************/	
RTMPMetaData::RTMPMetaData(QWORD timestamp)
{
	//Store timestamp
	this->timestamp = timestamp;
}

RTMPMetaData* RTMPMetaData::Clone()
{
	RTMPMetaData* obj = new RTMPMetaData(timestamp);
	//for each extra param
	for(DWORD i=0;i<params.size();i++)
		//Clone and add
		obj->AddParam(params[i]->Clone());
	//Return
	return obj;
}


RTMPMetaData::~RTMPMetaData()
{
	//for each extra param
	for(DWORD i=0;i<params.size();i++)
		//Delete object
		delete(params[i]);
}


DWORD RTMPMetaData::Parse(BYTE *data,DWORD size)
{

	BYTE* buffer = data;
	DWORD bufferSize = size;

	while(bufferSize)
	{
		//Parse amf object
		DWORD len = parser.Parse(buffer,bufferSize);

		//Remove used data
		buffer += len;
		bufferSize -= len;

		//Check if we got another object
		if (parser.IsParsed())
		{
			//Get Object
			AMFData* obj = parser.GetObject();
			//Add param
			params.push_back(obj);
		}
	}

	return (buffer-data);
}

DWORD RTMPMetaData::GetSize()
{
	//Get base length
	DWORD len = 0;

	//Check if we have extr;	
	for(DWORD i=0;i<params.size();i++)
		//Get extra size
		len += params[i]->GetSize();

	//Return length
	return len;
}

DWORD RTMPMetaData::Serialize(BYTE* buffer,DWORD size)
{
	AMFNull null;

	//Calculate size
	DWORD len = GetSize();

	//Check if we have enought size
	if (size<len)
		//Throw error
		return -1;
	
	//Reset
	len = 0;

	//Check if we have params;	
	for(DWORD i=0;i<params.size();i++)
		len += params[i]->Serialize(buffer+len,size-len);

	return len;	

}

DWORD RTMPMetaData::GetParamsLength()
{
	return params.size();
}

AMFData* RTMPMetaData::GetParams(DWORD i)
{
	return params[i];
}

void RTMPMetaData::AddParam(AMFData* param)
{
	params.push_back(param);
}


void RTMPMetaData::Dump()
{
	Debug("[RTMPMetaData ts=%lld]\n",timestamp);
	for(DWORD i=0;i<params.size();i++)
		params[i]->Dump();
	Debug("[/RTMPMetaData]\n");
}
/*************************
 * MediaFrame
 *
 ************************/
RTMPMediaFrame::RTMPMediaFrame(Type type,QWORD timestamp,BYTE *data,DWORD size)
{
	this->type = type;
	this->timestamp = timestamp;
	this->bufferSize = size;
	//Create buffer with padding
	this->buffer = (BYTE*)malloc(bufferSize+16);
	//Copy
	memcpy(buffer,data,bufferSize);
	//Set media size
	mediaSize = size;
	//Set position to the begining
	this->pos = 0;
	//Empty padding
	memset(buffer+bufferSize,0,16);
}

RTMPMediaFrame::RTMPMediaFrame(Type type,QWORD timestamp,DWORD size)
{
	this->type = type;
	this->timestamp = timestamp;
	this->bufferSize = size;
	//Create buffer with padding
	this->buffer = (BYTE*)malloc(bufferSize+16);
	this->pos = 0;
	this->mediaSize = 0;
	//Empty padding
	memset(buffer+bufferSize,0,16);
}

DWORD RTMPMediaFrame::Parse(BYTE *data,DWORD size)
{
	DWORD len = size;
	//Check size
	if (pos+len>bufferSize)
	{
		//Error message
		char msg[1024];
		//Print it
		sprintf(msg,"Not enought size for parsing frame [bufferSize:%d,pos:%d,size:%d]\n",bufferSize,pos,size);
		//Error
		throw std::runtime_error(msg);
	}
	//ONly copy what we can eat
	memcpy(buffer+pos,data,len);
	//Increase pos
	pos+=len;
	//Retrun copied data
	return len;
}

DWORD RTMPMediaFrame::Serialize(BYTE* data,DWORD size)
{
	//Check if enought space
	if (size<bufferSize)
		//Failed
		return 0;

	//Copy data
	memcpy(data,buffer,mediaSize);
	
	//Exit
	return mediaSize;
}

RTMPMediaFrame::~RTMPMediaFrame()
{
	//Check buffer alwasy
	if (buffer)
		//Delete
		free(buffer);
}

void RTMPMediaFrame::Dump()
{
	//Dump
	Debug("[MediaFrame type:%d timestamp:%lld bufferSize:%d mediaSize:%d]\n",type,timestamp,bufferSize,mediaSize);
	if(bufferSize>8)
		::Dump(buffer,8);
	else
		::Dump(buffer,bufferSize);
	Debug("[/MediaFrame]\n",type,timestamp,bufferSize,mediaSize);
}

RTMPVideoFrame::RTMPVideoFrame(QWORD timestamp,DWORD size) : RTMPMediaFrame(Video,timestamp,size)
{
	//No header
	headerPos = 0;
}

RTMPVideoFrame::RTMPVideoFrame(QWORD timestamp,const AVCDescriptor &desc) : RTMPMediaFrame(Video,timestamp, desc.GetSize())
{
	//Set type
	SetVideoCodec(RTMPVideoFrame::AVC);
	//Set type
	SetFrameType(RTMPVideoFrame::INTRA);
	//Set NALU type
	SetAVCType(AVCHEADER);
	//Set no delay
	SetAVCTS(0);
	//Serialize
	mediaSize = desc.Serialize(buffer,bufferSize);
}

RTMPVideoFrame::~RTMPVideoFrame()
{
	//No header
	headerPos = 0;
}

void RTMPVideoFrame::Dump()
{
	//Dump
	Debug("[VideoFrame codec:%d intra:%d timestamp:%lld bufferSize:%d mediaSize:%d]\n",codec,frameType,timestamp,bufferSize,mediaSize);
	if (codec==AVC)
		Debug("\t[AVC header 0x%.2x 0x%.2x 0x%.2x 0x%.2x /]\n",extraData[0],extraData[1],extraData[2],extraData[3]);
	if (codec==AVC && extraData[0]==AVCHEADER)
	{
		//Paser AVCDesc
		AVCDescriptor desc;
		//Parse it
		desc.Parse(buffer,bufferSize);
		//Dump it
		desc.Dump();

	} else if(bufferSize>16) {
		//Dump first 16 bytes only
		::Dump(buffer,16);
	} else {
		::Dump(buffer,bufferSize);
	}
	Debug("[/VideoFrame]\n",type,timestamp,bufferSize,mediaSize);
}

DWORD RTMPVideoFrame::Parse(BYTE *data,DWORD size)
{
	BYTE* buffer = data;
	DWORD bufferLen = size;

	//check size
	if(!size)
		//return consumed
		return size;

	//If it is the first
	if (!headerPos)
	{
		//Parse type
		codec = (VideoCodec) (buffer[0] & 0x0f);
		frameType = (FrameType) ((buffer[0] & 0xf0) >> 4);
		//Remove from data
		buffer++;
		bufferLen--;
		//INcrease header
		headerPos++;
	}

	//check size
	if(!bufferLen)
		//return consumed
		return size;

	//Check codec type
	if (headerPos<5 && codec==AVC)
	{
		//How much is left
		int len = 5-headerPos;
		//Copy data available
		if (len>bufferLen)
			len = bufferLen;
		//Copy
		memcpy(extraData+headerPos-1,buffer,len);
		//Skip next 4 bytes
		buffer	  += len;
		bufferLen -= len;
		//Inc header
		headerPos += len;
	}
	DWORD parsed = 0;

	//Check size
	if (bufferLen)
	{
		//Parse the rest
		parsed = RTMPMediaFrame::Parse(buffer,bufferLen);
		//Increase media data
		mediaSize += parsed;
	}

	//Return parsed data
	return parsed+(buffer-data);
}

DWORD RTMPVideoFrame::Serialize(BYTE* data,DWORD size)
{

	DWORD extra = 0;

	//Check codec
	if (codec==AVC)
		//Extra header
		extra = 4;

	//Check if enought space
	if (size<mediaSize+1+extra)
		//Failed
		return 0;

	//Set first byte
	data[0] = (frameType <<4 ) | (codec & 0x0f);

	//If it is AVC
	if (codec==AVC)
	{
		data[1] = extraData[0];
		data[2] = extraData[1];
		data[3] = extraData[2];
		data[4] = extraData[3];
	}

	//Copy media
	memcpy(data+1+extra,buffer,mediaSize);

	//Exit
	return mediaSize+1+extra;
}
DWORD RTMPVideoFrame::GetSize()
{
	//If it is not AVC
	if (codec!=AVC)
		//Check if enought space
		return mediaSize+1;
	else
		//Check if enought space
		return mediaSize+1+4;
}

DWORD RTMPVideoFrame::SetVideoFrame(BYTE* data,DWORD size)
{
	//Check if enought space
	if (size>bufferSize)
		//Failed
		return 0;

	//Copy media
	memcpy(buffer,data,size);

	//Set media sie
	mediaSize = size;

	//Exit
	return mediaSize;
}


RTMPMediaFrame *RTMPVideoFrame::Clone()
{
	RTMPVideoFrame *frame =  new RTMPVideoFrame(timestamp,mediaSize);
	//Set values
	frame->SetVideoCodec(codec);
	frame->SetFrameType(frameType);
	frame->SetVideoFrame(buffer,mediaSize);
	//Copy extra data
	memcpy(frame->extraData,extraData,4);
	//Return frame
	return frame;
}

RTMPAudioFrame::RTMPAudioFrame(QWORD timestamp,const AACSpecificConfig &config) : RTMPMediaFrame(Audio,timestamp,config.GetSize())
{
	//Set AAC codec properties
	SetAudioCodec(AAC);
        SetSoundRate(RTMPAudioFrame::RATE44khz);
        SetSamples16Bits(1);
        SetStereo(1);
	//Set type
	SetAACPacketType(AACSequenceHeader);
	//Set data
	SetAudioFrame(config.GetData(),config.GetSize());
}

RTMPAudioFrame::RTMPAudioFrame(QWORD timestamp,DWORD size) : RTMPMediaFrame(Audio,timestamp,size)
{
        //No header for parsing
	headerPos = 0;
}

RTMPAudioFrame::~RTMPAudioFrame()
{
}

void RTMPAudioFrame::Dump()
{
	//Dump
	Debug("[AudioFrame type:%d codec:%d rate:%d sample16bits:%d stereo:%d timestamp:%lld bufferSize:%d mediaSize:%d]\n",type,codec,rate,sample16bits,stereo,timestamp,bufferSize,mediaSize);
	if(bufferSize>8)
		::Dump(buffer,8);
	else
		::Dump(buffer,bufferSize);
	Debug("[/AudioFrame]\n");
}

DWORD RTMPAudioFrame::Parse(BYTE *data,DWORD size)
{
	BYTE* buffer = data;
	DWORD bufferLen = size;

        //Check size
        if (!size)
                //Done
                return size;
        
	//If it is the first
	if (!pos)
	{
		//Parse type
		codec		= (AudioCodec)(buffer[0]>>4);
		rate		= (SoundRate)((buffer[0]>>2) & 0x03);
		sample16bits	= ((buffer[0]>>1) & 0x01);
		stereo		=  buffer[0] & 0x01;
		//Remove from data
		buffer++;
		bufferLen--;
	}

        //Check still something
        if (!bufferLen)
                //Done so far
                return size;

	//Check AAC
	if (codec==AAC && !headerPos)
	{
		//Get type
		extraData[0] = buffer[0];
		//Remove from data
		buffer++;
		bufferLen--;
                //Increase header pos
                headerPos++;
	}
	//Parse the rest
	DWORD len = RTMPMediaFrame::Parse(buffer,bufferLen);
	//Increase media data
	mediaSize += len;

	//Return parsed data
	return len+(buffer-data);
}

DWORD RTMPAudioFrame::Serialize(BYTE* data,DWORD size)
{
	int pos = 0;

	//Check if enought space
	if (size<GetSize())
		//Failed
		return 0;

	//Set first byte
	data[pos++] = (codec<<4) | (rate<<2) | (sample16bits<<1) | stereo;

	//Check codec
	if (codec==AAC)
		//Se type
		data[pos++] = extraData[0];

	//Copy media
	memcpy(data+pos,buffer,mediaSize);

	//Exit
	return mediaSize+pos;
}
DWORD RTMPAudioFrame::GetSize()
{
	//Check if extradata is used
	if (codec!=AAC)
		//Check if enought space
		return mediaSize+1;
	else
		//Check if enought space
		return mediaSize+2;
}

DWORD RTMPAudioFrame::SetAudioFrame(const BYTE* data,DWORD size)
{
	//Check if enought space
	if (size>bufferSize)
		//Failed
		return 0;

	//Copy media
	memcpy(buffer,data,size);

	//Set media sie
	mediaSize = size;

	//Exit
	return mediaSize;
}

RTMPMediaFrame *RTMPAudioFrame::Clone()
{
	RTMPAudioFrame *frame =  new RTMPAudioFrame(timestamp,mediaSize);
	//Set values
	frame->SetAudioCodec(codec);
	frame->SetSoundRate(rate);
	frame->SetSamples16Bits(sample16bits);
	frame->SetStereo(stereo);
	frame->SetAACPacketType(GetAACPacketType());
	frame->SetAudioFrame(buffer,mediaSize);
	//Return frame
	return frame;
}


