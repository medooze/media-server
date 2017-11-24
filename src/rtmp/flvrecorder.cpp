#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "assertions.h"
#include "rtmp/flvrecorder.h"


FLVRecorder::FLVRecorder()
{
	//Not recording or playing
	fd = -1;
	recording = 0;
	first = 0;
	last = 0;
}

FLVRecorder::~FLVRecorder()
{
	//Close it
	Close();
}

bool FLVRecorder::Create(const char *filename)
{
	//If already playing
	if (fd!=-1)
		//Already opened
		return Error("Already opened\n");

	//Open file
	fd = open(filename,O_CREAT|O_WRONLY|O_TRUNC, 0664);

	//Check fd
	if (fd<0)
		return Error("Could not create file [%d,%s]\n",errno,filename);

	//Everythin ok
	return 1;
}

bool FLVRecorder::Record()
{
	FLVHeader header;
	FLVTag tag;
	FLVTagSize tagSize;
	UTF8Parser parser;

	//If not opened correctly
	if (fd<0)
		//Return error
		return Error("Recording error, file not openeed\n");

	//We are recording
	recording = true;
	first = 0;

	//Get creation date
	time_t ltime;
	ltime=time(NULL); /* get current cal time */

	//Print the timestamp
	parser.SetString(asctime(localtime(&ltime)));

	//Set header values
	header.SetTag((BYTE*)"FLV");
	header.SetVersion(1);
	header.SetMedia(5); //AUDIO+VIDEO
	header.SetHeaderOffset(9);

	//Set previous tag size (cero as it is the fist)
	tagSize.SetTagSize(0);

	//write header and tag size
	write(fd,header.GetData(),header.GetSize());
	write(fd,tagSize.GetData(),tagSize.GetSize());

	//Create metadata object
	meta = new RTMPMetaData(0);

	//Set name
	meta->AddParam(new AMFString(L"onMetaData"));

	//Create properties string
	AMFEcmaArray *prop = new AMFEcmaArray();

	//Add default properties
	prop->AddProperty(L"audiocodecid"	,0.0	);	//Number Audio codec ID used in the file (see E.4.2.1 for available SoundFormat values)
	prop->AddProperty(L"audiodatarate"	,0.0	);	// Number Audio bit rate in kilobits per second
	prop->AddProperty(L"audiodelay"		,0.0	);	// Number Delay introduced by the audio codec in seconds
	prop->AddProperty(L"audiosamplerate"	,0.0	);	// Number Frequency at which the audio stream is replayed
	prop->AddProperty(L"audiosamplesize"	,0.0	);	// Number Resolution of a single audio sample*/
	prop->AddProperty(L"canSeekToEnd"	,0.0	);	// Boolean Indicating the last video frame is a key frame
	prop->AddProperty(L"creationdate"	,parser.GetWChar()	);	// String Creation date and time
	prop->AddProperty(L"duration"		,0.0	);	// Number Total duration of the file in seconds
	prop->AddProperty(L"filesize"		,0.0	);	// Number Total size of the file in bytes
	prop->AddProperty(L"framerate"		,0.0	);	// Number Number of frames per second
	prop->AddProperty(L"height"		,0.0	);	// Number Height of the video in pixels
	prop->AddProperty(L"stereo"		,new AMFBoolean(false)	);	// Boolean Indicating stereo audio
	prop->AddProperty(L"videocodecid"	,0.0	);	// Number Video codec ID used in the file (see E.4.3.1 for available CodecID values)
	prop->AddProperty(L"videodatarate"	,0.0	);	// Number Video bit rate in kilobits per second
	prop->AddProperty(L"width"		,0.0	);	// Number Width of the video in pixels

	//Add param
	meta->AddParam(prop);

	//Allocate memory to store data
	DWORD size = meta->GetSize();
	BYTE *data = (BYTE*)malloc(size);

	//Serialize meta
	DWORD len = meta->Serialize(data,size);

	//Create tag
	tag.SetType(RTMPMessage::Data);
        tag.SetDataSize(len);
        tag.SetTimestamp(0);
        tag.SetTimestampExt(0);
        tag.SetStreamId(0);

	//Write header
	write(fd,tag.GetData(),tag.GetSize());

	//Get current file position
	offset = lseek(fd,0,SEEK_CUR);

	//Write meta data
	write(fd,data,len);

	//Free memory from metadata
	free(data);

	//Set tag size
	tagSize.SetTagSize(len+tag.GetSize());

	//Write back pointer size
	write(fd,tagSize.GetData(),tagSize.GetSize());

	return true;
}

bool FLVRecorder::Write(RTMPMediaFrame *frame)
{
	FLVTag tag;
	FLVTagSize tagSize;

	//If we are not recording
	if (!recording)
		//exit
		return 0;

	//Check size
	if (!frame->GetSize())
		//Exit
		return 0;

	//If it is the first frame
	if (!first)
		//Get timestamp
		first = frame->GetTimestamp();

	//Get timestamp
	last = frame->GetTimestamp()-first;

	//Allocate memory to store frame data
	DWORD size = frame->GetSize();
	BYTE *data = (BYTE*)malloc(size);

	//Serialize
	DWORD len = frame->Serialize(data,size);

	//Create tag
	tag.SetType(frame->GetType());
        tag.SetDataSize(len);
        tag.SetTimestamp(last);
        tag.SetTimestampExt(0);
        tag.SetStreamId(0);

	//Write tag
	write(fd,tag.GetData(),tag.GetSize());

	//Write frame
	write(fd,data,len);

	//Free data
	free(data);

	//Set full tag size
	tagSize.SetTagSize(tag.GetSize()+len);

	//Write back pointer size
	write(fd,tagSize.GetData(),tagSize.GetSize());

	return true;
}

bool FLVRecorder::Set(RTMPMetaData *setMetaData)
{
	//If we are not recording
	if (!recording)
		//exit
		return Error("Not recording\n");

	//Check lenght
	if (setMetaData->GetParamsLength()<2)
		//error
		return false;

	//Get properties object
	AMFData *param = setMetaData->GetParams(1);

	//Check type
	if (!param->CheckType(AMFData::EcmaArray))
		//Exit
		return false;

	//Get amf object
	AMFEcmaArray *arr = (AMFEcmaArray*)param;
	//Get metadata properties
	AMFEcmaArray *prop = (AMFEcmaArray*)meta->GetParams(1);

	//Get width
	if (arr->HasProperty(L"width"))
		prop->AddProperty(L"width",arr->GetProperty(L"width"));
	//Get height
	if (arr->HasProperty(L"height"))
		prop->AddProperty(L"height",arr->GetProperty(L"height"));
	//Get duration
	if (arr->HasProperty(L"duration"))
		prop->AddProperty(L"duration",arr->GetProperty(L"duration"));
	//Get audiocodecid
	if (arr->HasProperty(L"audiocodecid"))
		prop->AddProperty(L"audiocodecid",arr->GetProperty(L"audiocodecid"));
	//Get audiodatarate
	if (arr->HasProperty(L"audiodatarate"))
		prop->AddProperty(L"audiodatarate",arr->GetProperty(L"audiodatarate"));
	//Get audiosamplerate
	if (arr->HasProperty(L"audiosamplerate"))
		prop->AddProperty(L"audiosamplerate",arr->GetProperty(L"audiosamplerate"));
	//Get audiosamplesize
	if (arr->HasProperty(L"audiosamplesize"))
		prop->AddProperty(L"audiosamplesize",arr->GetProperty(L"audiosamplesize"));
	//Get framerate
	if (arr->HasProperty(L"framerate"))
		prop->AddProperty(L"framerate",arr->GetProperty(L"framerate"));
	//Get stereo
	if (arr->HasProperty(L"stereo"))
		prop->AddProperty(L"stereo",arr->GetProperty(L"stereo"));
	//Get videocodecid
	if (arr->HasProperty(L"videocodecid"))
		prop->AddProperty(L"videocodecid",arr->GetProperty(L"videocodecid"));
	//Get videodatarate
	if (arr->HasProperty(L"videodatarate"))
		prop->AddProperty(L"videodatarate",arr->GetProperty(L"videodatarate"));

	return true;
}

bool FLVRecorder::Stop()
{
        //Check if recording
        if (!recording)
                //Error
                return 0;

        //Not recording anymore
	recording = false;

        //Rewind
	lseek(fd,offset,SEEK_SET);

	//Check meta
	if (meta)
	{
		//Get metadata properties
		AMFEcmaArray *prop = (AMFEcmaArray*)meta->GetParams(1);

		//Set duration
		prop->AddProperty(L"duration",(float)last/1000);

		//Allocate memory to store data
		DWORD size = meta->GetSize();
		BYTE *data = (BYTE*)malloc(size);

		//Seralize metadata
		DWORD len = meta->Serialize(data,size);

		//Write meta
		write(fd,data,len);

		//Free memory
		free(data);

		//Free meta
		delete (meta);

		//Empty
		meta = NULL;
	}

        //OK
        return true;
}

bool FLVRecorder::Close()
{
	//Stop
        Stop();

        //Check fd
        if (fd<0)
                //Error
                return false;

	//Close file
	MCU_CLOSE(fd);

	//We are not playing or recording
	fd = -1;

        //OK
	return true;
}

