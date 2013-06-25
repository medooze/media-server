/* 
 * File:   flvdump.cpp
 * Author: Sergio
 *
 * Created on 25 de junio de 2013, 23:39
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "config.h"
#include "flv.h"
#include "rtmpmessage.h"

/*
 * 
 */
int main(int argc, char** argv) {

        BYTE data[1024];
	int len = 0;
	DWORD state = 0;

	FLVHeader header;
	FLVTag	tag;
	FLVTagSize tagSize;
	RTMPMessage *msg=NULL;
	QWORD lastTs = 0;

        //check args
        if (argc!=2)
        {
                //Show usage
                printf("FLV dump utility\r\n");
                printf("Usage: flvdump filename.flv\r\n\r\n");
                //Exit
                return 1;
        }


        //Get username
        char* filename = argv[1];

	//Open file
	int fd = open(filename,O_RDONLY);

	//Check fd
	if (fd==-1)
		//exit
		return Error("-Could not open file [%d,%s]\n",errno,filename);

        printf("Dumping %s\r\n",filename);

	//While there is still data in the file and we are playing
	while((len=read(fd,data,1024))>0)
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
                                                header.Dump();
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
                                                tagSize.Dump();
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
                                                tag.Dump();
						//Create new frame
						msg = new RTMPMessage(0,tag.GetTimestamp(),(RTMPMessage::Type)tag.GetType(),tag.GetDataSize());
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
						} else if (msg->IsMedia()) {
							//Get media frame
							RTMPMediaFrame *frame = msg->GetMediaFrame();
                                                        //Dump fraame
                                                        frame->Dump();
							//Get timestamp
							QWORD ts = frame->GetTimestamp();
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

        return 0;
}

