#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include "textstream.h"
#include "log.h"
#include "tools.h"

static BYTE BOMUTF8[]			= {0xEF,0xBB,0xBF};
static BYTE LOSTREPLACEMENT[]		= {0xEF,0xBF,0xBD};

/**********************************
* TextStream
*	Constructor
***********************************/
TextStream::TextStream(RTPSession::Listener* listener) : rtp(MediaFrame::Text,listener)
{
	sendingText=0;
	receivingText=0;
	textCodec=TextCodec::T140;
	muted = false;
}

/*******************************
* ~TextStream
*	Destructor. 
********************************/
TextStream::~TextStream()
{
}

/***************************************
* SetTextCodec
*	Fija el codec de text
***************************************/
int TextStream::SetTextCodec(TextCodec::Type codec)
{
	//Compromabos que soportamos el modo
	if (!(codec==TextCodec::T140 || codec==TextCodec::T140RED))
		return 0;

	//Colocamos el tipo de text
	textCodec = codec;

	Log("-SetTextCodec [%d,%s]\n",textCodec,TextCodec::GetNameFor(textCodec));

	//Y salimos
	return 1;	
}

/***************************************
* Init
*	Inicializa los devices 
***************************************/
int TextStream::Init(TextInput *input, TextOutput *output)
{
	Log(">Init text stream\n");

	//Iniciamos el rtp
	if(!rtp.Init())
		return Error("No hemos podido abrir el rtp\n");
	

	//Nos quedamos con los puntericos
	textInput  = input;
	textOutput = output;

	//Y aun no estamos mandando nada
	sendingText=0;
	receivingText=0;

	Log("<Init text stream\n");

	return 1;
}

int TextStream::SetLocalCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetLocalCryptoSDES(suite,key64);
}

int TextStream::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetRemoteCryptoSDES(suite,key64);
}

int TextStream::SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint)
{
	return rtp.SetRemoteCryptoDTLS(setup,hash,fingerprint);
}

int TextStream::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetLocalSTUNCredentials(username,pwd);
}

int TextStream::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetRemoteSTUNCredentials(username,pwd);
}
int TextStream::SetRTPProperties(const Properties& properties)
{
	return rtp.SetProperties(properties);
}
/***************************************
* startSendingText
*	Helper function
***************************************/
void * TextStream::startSendingText(void *par)
{
	TextStream *conf = (TextStream *)par;
	blocksignals();
	Log("SendTextThread [%p]\n",pthread_self());
	conf->SendText();
	//Exit
	return NULL;
}

/***************************************
* startReceivingText
*	Helper function
***************************************/
void * TextStream::startReceivingText(void *par)
{
	TextStream *conf = (TextStream *)par;
	blocksignals();
	Log("RecvTextThread [%p]\n",pthread_self());
	conf->RecText();
	//Exit
	return NULL;
}

/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int TextStream::StartSending(char *sendTextIp,int sendTextPort,const RTPMap& rtpMap,const RTPMap& aptMap)
{
	Log(">StartSending text [%s,%d]\n",sendTextIp,sendTextPort);

	//Si estabamos mandando tenemos que parar
	if (sendingText)
		//paramos
		StopSending();
	
	//Si tenemos text
	if (sendTextPort==0)
		//Error
		return Error("Text port 0\n");


	//Y la de text
	if(!rtp.SetRemotePort(sendTextIp,sendTextPort))
		//Error
		return Error("Error en el SetRemotePort\n");

	//Set sending map
	rtp.SetSendingRTPMap(rtpMap,aptMap);

	//Get t140 for redundancy
	for (RTPMap::const_iterator it = rtpMap.begin(); it!=rtpMap.end(); ++it)
	{
		//Is it ourr codec
		if (it->second==TextCodec::T140)
		{
			//Set it
			t140Codec = it->first;
			//and we are done
			continue;
		}
	}

	//Set text codec
	if(!rtp.SetSendingCodec(textCodec))
		//Error
		return Error("%s text codec not supported by peer\n",TextCodec::GetNameFor(textCodec));

	//Arrancamos el thread de envio
	sendingText=1;

	//Start thread
	createPriorityThread(&sendTextThread,startSendingText,this,1);

	Log("<StartSending text [%d]\n",sendingText);

	return 1;
}

/***************************************
* StartReceiving
*	Abre los sockets y empieza la recetpcion
****************************************/
int TextStream::StartReceiving(const RTPMap& rtpMap,const RTPMap& aptMap)
{
	//If already receiving
	if (receivingText)
		//Stop it
		StopReceiving();

	//Get local rtp port
	int recTextPort = rtp.GetLocalPort();

	//Set receving map
	rtp.SetReceivingRTPMap(rtpMap,aptMap);

	//We are reciving text
	receivingText=1;

	//Create thread
	createPriorityThread(&recTextThread,startReceivingText,this,1);

	//Log
	Log("<StartReceiving text [%d]\n",recTextPort);

	//Return receiving port
	return recTextPort;
}

/***************************************
* End
*	Termina la conferencia activa
***************************************/
int TextStream::End()
{
	//Terminamos de enviar
	StopSending();

	//Y de recivir
	StopReceiving();

	//Cerramos la session de rtp
	rtp.End();

	return 1;
}

/***************************************
* StopReceiving
* 	Termina la recepcion
****************************************/

int TextStream::StopReceiving()
{
	Log(">StopReceiving Text\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingText)
	{	
		//Paramos de enviar
		receivingText=0;

		//Cancel rtp
		rtp.CancelGetPacket();
		
		//Y unimos
		pthread_join(recTextThread,NULL);
	}

	Log("<StopReceiving Text\n");

	return 1;

}

/***************************************
* StopSending
* 	Termina el envio
****************************************/
int TextStream::StopSending()
{
	Log(">StopSending Text\n");

	//Esperamos a que se cierren las threads de envio
	if (sendingText)
	{
		//paramos
		sendingText=0;

		//Cancel grab if any
		textInput->Cancel();

		//Y esperamos
		pthread_join(sendTextThread,NULL);
	}

	Log("<StopSending Text\n");

	return 1;	
}


/****************************************
* RecText
*	Obtiene los packetes y los muestra
*****************************************/
int TextStream::RecText()
{
	BYTE*		text;
	DWORD		textSize;
	RedHeaders	headers;
	DWORD		lastSeq = RTPPacket::MaxExtSeqNum;

	Log(">RecText\n");

	//Mientras tengamos que capturar
	while(receivingText)
	{
		//Get packet
		auto packet = rtp.GetPacket();

		//Check if gor anti
		if (!packet)
			//Next
			continue;

		//Get type
		TextCodec::Type type = (TextCodec::Type)packet->GetCodec();

		//Get timestamp
		DWORD timeStamp = packet->GetTimestamp();

		//Get extended sequence number
		DWORD seq = packet->GetExtSeqNum();

		//Lost packets since last one
		DWORD lost = 0;

		//If not first
		if (lastSeq!=RTPPacket::MaxExtSeqNum)
			//Calculate losts
			lost = seq-lastSeq-1;

		//Update last sequence number
		lastSeq = seq;

		//Check if we are muted
		if (!muted)
		{
			//Check the type of data
			if (type==TextCodec::T140RED)
			{
				//Get redundant packet
				auto red = std::static_pointer_cast<RTPRedundantPacket>(packet);
				
				//Timestamp of first packet (either receovered or not)
				DWORD ts = timeStamp;
					
				//Check if we have any red pacekt
				if (red->GetRedundantCount()>0)
					//Get the timestamp of first redundant packet
					ts = red->GetRedundantTimestamp(0);

				//For each lonot recoveredt packet send a mark
				for (int i=red->GetRedundantCount();i<lost;i++)
				{

					//Create frame of lost replacement
					TextFrame frame(ts,LOSTREPLACEMENT,sizeof(LOSTREPLACEMENT));
					//Y lo reproducimos
					textOutput->SendFrame(frame);
				}

				//If we have lost too many
				if (lost>red->GetRedundantCount())
					//Get what we have available only
					lost = red->GetRedundantCount();
				//Fore each recovered packet
				for (int i=red->GetRedundantCount()-lost;i<red->GetRedundantCount();i++)
				{
					//Create frame from recovered data
					TextFrame frame(red->GetRedundantTimestamp(i),red->GetRedundantPayloadData(i),red->GetRedundantPayloadSize(i));
					//Y lo reproducimos
					textOutput->SendFrame(frame);
				}
				// Now process the primary data
				TextFrame frame(timeStamp,red->GetPrimaryPayloadData(),red->GetPrimaryPayloadSize());
				//Send it
				textOutput->SendFrame(frame);
			} else {
				//For each lost packet send a mark
				for (int i=0;i<lost;i++)
				{
					//Create frame of lost replacement
					TextFrame frame(timeStamp,LOSTREPLACEMENT,sizeof(LOSTREPLACEMENT));
					//Y lo reproducimos
					textOutput->SendFrame(frame);
				}
				//Create frame
				TextFrame frame(timeStamp,packet->GetMediaData(),packet->GetMediaLength());
				//Send it
				textOutput->SendFrame(frame);
			}
		}

	}

	Log("<RecText\n");
}

/*******************************************
* SendText
*	Capturamos el text y lo mandamos
*******************************************/
int TextStream::SendText()
{
	bool idle = true;
	DWORD timeout = 25000;
	DWORD lastTime = 0;

	Log(">SendText\n");

	//Mientras tengamos que capturar
	while(sendingText)
	{
		//Text frame
		TextFrame *frame = NULL;

		//Get frame
		frame = textInput->GetFrame(timeout);

		//Calculate last frame time
		if (frame)
			//Get it from frame
			lastTime = frame->GetTimeStamp();
		else
			//Update last send time with timeout
			lastTime += timeout;
		
		//Create packet
		RTPPacket::shared packet = std::make_shared<RTPPacket>(MediaFrame::Text,textCodec);

		//Set timestamp
		packet->SetTimestamp(lastTime);

		//Check codec
		if (textCodec==TextCodec::T140)
		{
			//Check frame
			if (frame)
			{
				//Set data
				packet->SetPayload(frame->GetData(),frame->GetLength());

				//Set Mark for the first frame after idle
				packet->SetMark(idle);

				//Send it
				rtp.SendPacket(packet);
				
				//Delete frame
				delete(frame);

				//Not idle anymore
				idle = false;
				//Set timeout to normal 300 ms
				timeout = 300;
			} else if (!idle) {
				//We start an idle period
				idle = true;
				//Set timeout to 25 seconds to send the keepalive minus the already waited time
				timeout = 25000;
			} else {
				//Set data
				packet->SetPayload(BOMUTF8,sizeof(BOMUTF8));

				//No mark
				packet->SetMark(false);

				//Send it
				rtp.SendPacket(packet);
			}
		} else {
			BYTE buffer[MTU] ZEROALIGNEDTO32;
			//Get first
			BYTE* red = buffer;
			//Init buffer length
			DWORD bufferLen = 0;
			//Total red size
			DWORD redLen = 0;
			//Calculate total red size without headers
			for (RedFrames::iterator it = reds.begin();it!=reds.end();++it)
				//Add packet payload
				redLen = (*it)->GetLength();
			//Now remove until it fits in an rtp payload (taking red headers into account)
			while (frame && redLen+frame->GetLength()+8>RTPPAYLOADSIZE)
			{
				//Get packe
				TextFrame* r = reds.front();
				//Dequeue it
				reds.pop_front();
				//Reduce red size
				redLen -= r->GetLength();
				//Delete it
				delete(r);
			}

			//Fill with empty redundant packets
			for (int i=0;i<2-reds.size();i++)
			{
				//Empty t140 redundancy packet
				red[0] = 0x80 | t140Codec;
				//Randomize time
				red[1] = rand();
				red[2] = (rand() & 0x3F) << 2;
				//No size
				red[3] = 0;
				//Increase buffer
				red += 4;
				bufferLen += 4;
			}

			//Iterate to put the header blocks
			for (RedFrames::iterator it = reds.begin();it!=reds.end();++it)
			{
				//Get frame and go to next
				TextFrame *f = *(it);
				//Calculate diff
				DWORD diff = lastTime-f->GetTimeStamp();
				/****************************************************
				 *  F: 1 bit First bit in header indicates whether another header block
				 *     follows.  If 1 further header blocks follow, if 0 this is the
				 *      last header block.
				 *
				 *  block PT: 7 bits RTP payload type for this block.
				 *
				 *  timestamp offset:  14 bits Unsigned offset of timestamp of this block
				 *      relative to timestamp given in RTP header.  The use of an unsigned
				 *      offset implies that redundant data must be sent after the primary
				 *      data, and is hence a time to be subtracted from the current
				 *      timestamp to determine the timestamp of the data for which this
				 *      block is the redundancy.
				 *
				 *  block length:  10 bits Length in bytes of the corresponding data
				 *      block excluding header.
				 ********************************/
				red[0] = 0x80 | t140Codec;
				red[1] = diff >> 6;
				red[2] = (diff & 0x3F) << 2;
				red[2] |= f->GetLength() >> 8;
				red[3] = f->GetLength();
				//Increase buffer
				red += 4;
				bufferLen += 4;
			}
			//Set primary encoded data and last mark
			red[0] = t140Codec;
			//Increase buffer
			red++;
			bufferLen++;
			
			//Iterate to put the redundant data
			for (RedFrames::iterator it = reds.begin();it!=reds.end();++it)
			{
				//Get frame and go to next
				TextFrame *f = *(it);
				//Copy
				memcpy(red,f->GetData(),f->GetLength());
				//Increase sizes
				red += f->GetLength();
				bufferLen += f->GetLength();
			}

			//Check if there is frame
			if (frame)
			{
				//Copy
				memcpy(red,frame->GetData(),frame->GetLength());
				//Serialize data
				bufferLen += frame->GetLength();
				//Push frame to the redundancy queue
				reds.push_back(frame);
			} else {
				//Push new empty frame
				reds.push_back(new TextFrame(lastTime,(wchar_t*)NULL,0));
			}
			//Send the mark bit if it is first frame after idle
			bool mark = idle && frame;

			//Set data
			packet->SetPayload(buffer,bufferLen);

			//Set timestamp
			packet->SetTimestamp(lastTime);

			//Set mark
			packet->SetMark(mark);

			//Send it
			rtp.SendPacket(packet);

			//Check size of the queue
			if (reds.size()==3)
			{
				//Delete first
				delete(reds.front());
				//Dequeue
				reds.pop_front();
			}

			//Calculate timeouts
			if (frame)
			{
				//Not idle anymore
				idle = false;
				//Normal timeout
				timeout = 300;
			} else {
				//Mark as idle as default
				idle = true;
				//By default wait for keep-alive
				timeout = 25000;
				//Check for non em
				for (RedFrames::iterator it = reds.begin();it!=reds.end();++it)
				{
					//Get frame and go to next
					TextFrame *f = *(it);
					//If it is not empty
					if (f->GetLength())
					{
						//Mark as idle
						idle = true;
						//Send redundant packet in 300 ms
						timeout = 300;
					}
				}
			}
		}
	}

	//Salimos
	Log("<SendText\n");
}

MediaStatistics TextStream::GetStatistics()
{
	MediaStatistics stats;

	//Fill stats
	stats.isReceiving	= IsReceiving();
	stats.isSending		= IsSending();
	stats.lostRecvPackets   = rtp.GetLostRecvPackets();
	stats.numRecvPackets	= rtp.GetNumRecvPackets();
	stats.numSendPackets	= rtp.GetNumSendPackets();
	stats.totalRecvBytes	= rtp.GetTotalRecvBytes();
	stats.totalSendBytes	= rtp.GetTotalSendBytes();

	//Return it
	return stats;
}

int TextStream::SetMute(bool isMuted)
{
	//Set it
	muted = isMuted;
	//Exit
	return 1;
}
