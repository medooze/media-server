#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "assertions.h"
#include "rtmp/rtmpflvstream.h"

class FLVHeader : public RTMPFormat<9>
{
public:
	BYTE*   GetTag()        	{ return data;	       }
        BYTE    GetVersion()  		{ return get1(data,3); }
        BYTE    GetMedia()      	{ return get1(data,4); }
        BYTE    GetHeaderOffset()	{ return get4(data,5); }

	void 	SetTag(BYTE* tag)	{ memcpy(data,tag,3);	}
        void    SetVersion(BYTE version){ set1(data,3,version);	}
        void    SetMedia(BYTE media)   	{ set1(data,4,media); 	}
        void    SetHeaderOffset(DWORD o){ set4(data,5,o); 	}
};

class FLVTag : public RTMPFormat<11>
{
public:
	BYTE    GetType()        	{ return get1(data,0); }
        DWORD   GetDataSize()  		{ return get3(data,1); }
        DWORD   GetTimestamp()      	{ return get3(data,4); }
        BYTE    GetTimestampExt()	{ return get1(data,7); }
        DWORD   GetStreamId()		{ return get3(data,8); }

	void    SetType(BYTE type)    	 { set1(data,0,type);	}
        void    SetDataSize(DWORD size)  { set3(data,1,size);	}
        void    SetTimestamp(DWORD ts)   { set3(data,4,ts);	}
        void    SetTimestampExt(BYTE ext){ set1(data,7,ext);	}
        void    SetStreamId(DWORD id)	 { set3(data,8,id);	}
};

class FLVTagSize : public RTMPFormat<4>
{
public:
        DWORD   GetTagSize()  		{ return get4(data,0);	}
        void    SetTagSize(DWORD size)	{ set4(data,0,size);	}
};


RTMPFLVStream::RTMPFLVStream(DWORD id) : RTMPMediaStream(id)
{
	//Not recording or playing
	fd = -1;
	recording = 0;
	playing = 0;
}

RTMPFLVStream::~RTMPFLVStream()
{
	//If we had a file opened
	if (fd!=-1)
		//Close it
		Close();
}

bool RTMPFLVStream::Play(std::wstring& url)
{
	char filename[1024];

	//If already playing
	if (fd!=-1)
		//Close it and start a new playback
		Close();

	//get file name
	snprintf(filename,1024,"%ls",url.c_str());

	//Open file
	fd = open(filename,O_RDONLY);

	//Check fd
	if (fd==-1)
	{
		//Send error comand
		SendCommand(L"onStatus", new RTMPNetStatusEvent(L"NetStream.Play.StreamNotFound",L"error",L"Stream not found"));
		//exit
		return Error("-Could not open file [%d,%s]\n",errno,filename);
	}

	//We are playing
	playing = true;

	//Send play comand
	SendCommand(L"onStatus", new RTMPNetStatusEvent(L"NetStream.Play.Reset",L"status",L"Playback reset") );

	//Send play comand
	SendCommand(L"onStatus", new RTMPNetStatusEvent(L"NetStream.Play.Start",L"status",L"Playback started") );

	//Start thread
	createPriorityThread(&thread,play,this,0);

	return true;
}

bool RTMPFLVStream::Publish(std::wstring& url)
{
	char filename[1024];

	//If already playing
	if (fd!=-1)
		//Close it and start a new playback
		Close();

	//get file name
	snprintf(filename,1024,"%ls",url.c_str());

	//Open file
	fd = open(filename,O_CREAT|O_WRONLY|O_TRUNC,0666);

	//Check fd
	if (!fd)
		return Error("-Could not open file [%d,%s]\n",errno,filename);

	Log("-Recording [%s]\n",filename);

	//We are recording
	recording = true;
	first = 0;

	FLVHeader header;
	FLVTagSize tagSize;

	header.SetTag((BYTE*)"FLV");
	header.SetVersion(1);
	header.SetMedia(5); //AUDIO+VIDEO
	header.SetHeaderOffset(9);

	//Set previous tag size (cero as it is the fist)
	tagSize.SetTagSize(0);

	//write header and tag size
	write(fd,header.GetData(),header.GetSize());
	write(fd,tagSize.GetData(),tagSize.GetSize());

	return true;
}

void RTMPFLVStream::PublishMediaFrame(RTMPMediaFrame *frame)
{
	FLVTag tag;
	FLVTagSize tagSize;

	//If we are not recording
	if (!recording)
		//exit
		return;

	//If it is the first frame
	if (!first)
		//Get timestamp
		first = frame->GetTimestamp();

	//Create tag
	tag.SetType(frame->GetType());
        tag.SetDataSize(frame->GetSize());
        tag.SetTimestamp(frame->GetTimestamp()-first);
        tag.SetTimestampExt(0);
        tag.SetStreamId(0);

	//Set tag size
	tagSize.SetTagSize(frame->GetSize()+tag.GetSize());

	//Write header
	write(fd,tag.GetData(),tag.GetSize());

	//Allocate memory to store frame data
	DWORD size = frame->GetSize();
	BYTE *data = (BYTE*)malloc(size);

	//Serialize
	DWORD len = frame->Serialize(data,size);

	//Write frame
	write(fd,data,len);

	//Free data
	free(data);

	//Write back pointer size
	write(fd,tagSize.GetData(),tagSize.GetSize());
}

void RTMPFLVStream::PublishMetaData(RTMPMetaData *meta)
{
	//If we are not recording
	if (!recording)
		//exit
		return;

	//Check number of objet
	if (!meta->GetParamsLength())
	{
		//Exit
		Error("Emtpy metadata\n");
		return;
	}

	//Get first param
	std::wstring name = *(meta->GetParams(0));


	//Check name
	if (name.compare(L"@setDataFrame")!=0)
	{
		Error("-Meta data unknown [%ls]\n",name.c_str());
		return;
	}

	//Allocate memory to store data
	DWORD size = meta->GetSize();
	BYTE *data = (BYTE*)malloc(size);
	DWORD len = 0;

	FLVTag tag;
	FLVTagSize tagSize;

	//Seralize the other objects except name
	for (DWORD i=1;i<meta->GetParamsLength();i++)
		//Serialize
		len +=  meta->GetParams(i)->Serialize(data+len,size-len);

	//Create tag
	tag.SetType(RTMPMessage::Data);
        tag.SetDataSize(len);
        tag.SetTimestamp(0);
        tag.SetTimestampExt(0);
        tag.SetStreamId(0);

	//Write header
	write(fd,tag.GetData(),tag.GetSize());

	//Set tag size
	tagSize.SetTagSize(len+tag.GetSize());

	//Write meta
	write(fd,data,len);

	//Write back pointer size
	write(fd,tagSize.GetData(),tagSize.GetSize());

	//Free memory
	free(data);
}

bool RTMPFLVStream::Close()
{
	//Check if it is playing or recording
	if (recording)
	{
		//Stop
		recording = false;
	} else {
		//Stop
		playing = false;
		//Join thread
		pthread_join(thread,NULL);
	}

	//Close file
	MCU_CLOSE(fd);

	//We are not playing or recording
	fd = -1;
}

int RTMPFLVStream::PlayFLV()
{
	BYTE data[1024];
	int len = 0;
	DWORD state = 0;

	FLVHeader header;
	FLVTag	tag;
	FLVTagSize tagSize;
	RTMPMessage *msg=NULL;
	QWORD lastTs = 0;

	Log(">PlayFLV\n");

	//Send stream begin
	SendStreamBegin();

	//While there is still data in the file and we are playing
	while((len=read(fd,data,1024))>0 && playing)
	{
		DWORD bufferLen = len;
		BYTE *buffer=data;

		//While we have still data in the file
		while (bufferLen)
		{
			switch (state)
			{
				case 0:
					//Parse the header
					len = header.Parse(buffer,bufferLen);
					//If we have readed it
					if (header.IsParsed())
					{
						//next state
						state = 1;
					}
					break;
				case 1:
					//Read tag size
					len = tagSize.Parse(buffer,bufferLen);
					//If we have readed it
					if (tagSize.IsParsed())
					{
						//Next header
						tag.Reset();
						//next state
						state = 2;
					}
					break;
				case 2:
					//Read tag size
					len = tag.Parse(buffer,bufferLen);
					//If we have readed it
					if (tag.IsParsed())
					{
						//Create new frame
						msg = new RTMPMessage(id,tag.GetTimestamp(),(RTMPMessage::Type)tag.GetType(),tag.GetDataSize());
						//next state
						state = 3;
					}
					break;
				case 3:
					//Read message
					len = msg->Parse(buffer,bufferLen);
					//If we have read it
					if (msg->IsParsed())
					{
						//If it is a metadata
						if (msg->IsMetaData())
						{
							//Get meta
							RTMPMetaData *meta = msg->GetMetaData();
							//Dump metadata
							meta->Dump();
							//If we got listener
							SendMetaData(meta);
						} else if (msg->IsMedia()) {
							//Get media frame
							RTMPMediaFrame *frame = msg->GetMediaFrame();

							//Get timestamp
							QWORD ts = frame->GetTimestamp();
							//If we got listener
							SendMediaFrame(frame);
							//Sleep
							msleep((ts-lastTs)*1000);
							//update
							lastTs=ts;
						}
						//Delete msg
						delete(msg);
						//Nullify
						msg = NULL;
						//Next flag size
						tagSize.Reset();
						//Next state
						state = 1;

					}
					break;
			}
			//Move pointers
			buffer += len;
			bufferLen -= len;
		}
	}

	//Check if we were already playing
	if (playing)
		//Send stream begin
		SendStreamEnd();

	Log("<PlayFLV\n");

	//If we have a message yet
	if(msg)
		//Delete it
		delete(msg);

	return 0;
}

void* RTMPFLVStream::play(void *par)
{
	Log("-PlayTrhead [%p]\n",pthread_self());

	//Obtenemos el parametro
	RTMPFLVStream *flv = (RTMPFLVStream *)par;

	//Bloqueamos las señales
	blocksignals();

	//Ejecutamos
	flv->PlayFLV();
	//Exit
	return NULL;
}

bool RTMPFLVStream::Seek(DWORD time)
{
	//Send error
	SendCommand(L"onStatus", new RTMPNetStatusEvent(L"NetStream.Seek.Failed",L"error",L"Seek not supported"));
	return false;
}
