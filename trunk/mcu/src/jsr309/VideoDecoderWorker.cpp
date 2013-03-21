/* 
 * File:   VideoDecoderWorker.cpp
 * Author: Sergio
 * 
 * Created on 2 de noviembre de 2011, 23:38
 */

#include "VideoDecoderWorker.h"
#include "rtp.h"
#include "log.h"

VideoDecoderJoinableWorker::VideoDecoderJoinableWorker()
{
	//Nothing
	output = NULL;
	joined = NULL;
	decoding = false;
}

VideoDecoderJoinableWorker::~VideoDecoderJoinableWorker()
{
	End();
}

int VideoDecoderJoinableWorker::Init(VideoOutput *output)
{
	//Store it
	this->output = output;
}

int VideoDecoderJoinableWorker::End()
{
	//Dettach
	Dettach();

	//Check if already decoding
	if (decoding)
		//Stop
		Stop();

	//Set null
	output = NULL;
}

int VideoDecoderJoinableWorker::Start()
{
	Log("-StartVideoDecoderJoinableWorker\n");

	//Check
	if (!output)
		//Exit
		return Error("null video output");

	//Check if need to restart
	if (decoding)
		//Stop first
		Stop();

	//Start decoding
	decoding = 1;

	//launc thread
	createPriorityThread(&thread,startDecoding,this,0);

	return 1;
}
void * VideoDecoderJoinableWorker::startDecoding(void *par)
{
	Log("VideoDecoderJoinableWorkerThread [%d]\n",getpid());
	//Get worker
	VideoDecoderJoinableWorker *worker = (VideoDecoderJoinableWorker *)par;
	//Block all signals
	blocksignals();
	//Run
	pthread_exit((void *)worker->Decode());
}

int  VideoDecoderJoinableWorker::Stop()
{
	Log(">StopVideoDecoderJoinableWorker\n");

	//If we were started
	if (decoding)
	{
		//Stop
		decoding=0;

		//Cancel any pending wait
		packets.Cancel();

		//Esperamos
		pthread_join(thread,NULL);
	}

	Log("<StopVideoDecoderJoinableWorker\n");

	return 1;
}


int VideoDecoderJoinableWorker::Decode()
{
	VideoDecoder*	videoDecoder = NULL;
	VideoCodec::Type type;
	int width  = 0;
	int height = 0;
	int lastSN = -1;
	bool lost = false;

	Log(">DecodeVideo\n");

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Obtenemos el paquete
		if (!packets.Wait(0))
			//Check condition again
			continue;

		//Get packet in queue
		RTPPacket* packet = packets.Pop();

		//Check
		if (!packet)
			//Check condition again
			continue;

		//Check
		if (lastSN!=-1 && lastSN+1!=packet->GetSeqNum())
			//No lost
			lost = false;
		else
			//Lost packet
			lost = true;

		//Update
		lastSN = packet->GetSeqNum();

		//Get new type
		type = (VideoCodec::Type)packet->GetCodec();
		
		//Comprobamos el tipo
		if ((videoDecoder==NULL) || (type!=videoDecoder->type))
		{
			//Si habia uno nos lo cargamos
			if (videoDecoder!=NULL)
				delete videoDecoder;

			//Creamos uno dependiendo del tipo
			if (!(videoDecoder = VideoCodecFactory::CreateDecoder(type)))
				continue;
		}

		//Lo decodificamos
		if(!videoDecoder->DecodePacket(packet->GetMediaData(),packet->GetMediaLength(),lost,packet->GetMark()))
			continue;

		//Si es el ultimo
		if(packet->GetMark())
		{
			//Comprobamos el tama�o
			if (width!=videoDecoder->GetWidth() || height!=videoDecoder->GetHeight())
			{
				//A cambiado o es la primera vez
				width = videoDecoder->GetWidth();
				height= videoDecoder->GetHeight();

				Log("-Changing video size %dx%d\n",width,height);

				//POnemos el modo de pantalla
				if (!output->SetVideoSize(width,height))
				{
					Error("Can't update video size\n");
					break;;
				}
			}

			//Get picture
			BYTE *frame = videoDecoder->GetFrame();

			//Check size
			if (width && height && frame)
				//Lo sacamos
				output->NextFrame(frame);
		}
	}

	//Borramos el encoder
	delete videoDecoder;

	Log("<DecodeVideo\n");

	//Exit
	return 0;
}

void VideoDecoderJoinableWorker::onRTPPacket(RTPPacket &packet)
{
	//Put it on the queue
	packets.Add(packet.Clone());
}

void VideoDecoderJoinableWorker::onResetStream()
{
	//Clean all packets
	packets.Clear();
}

void VideoDecoderJoinableWorker::onEndStream()
{
	//Stop decoding
	Stop();
	//Not joined anymore
	joined = NULL;
}

int VideoDecoderJoinableWorker::Attach(Joinable *join)
{
	//Detach if joined
	if (joined)
	{
		//Stop
		Stop();
		//Remove ourself as listeners
		joined->RemoveListener(this);
	}
	//Store new one
	joined = join;
	//If it is not null
	if (joined)
	{
		//Start
		Start();
		//Join to the new one
		join->AddListener(this);
	}
	//OK
	return 1;
}

int VideoDecoderJoinableWorker::Dettach()
{
        //Detach if joined
	if (joined)
	{
		//Stop decoding
		Stop();
		//Remove ourself as listeners
		joined->RemoveListener(this);
	}

	//Not joined anymore
	joined = NULL;
}
