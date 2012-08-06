#include "log.h"
#include "VideoMixerResource.h"

VideoMixerResource::VideoMixerResource(std::wstring &tag)
{
	//Store tag
	this->tag = tag;

	//Not inited
	inited = false;

	//Init part counter
	maxId = 1;
}

VideoMixerResource::~VideoMixerResource()
{
	//Check if ended properly
	if (inited)
		//End!!
		End();
}

int VideoMixerResource::Init(Mosaic::Type comp,int size)
{

	Log("-Init VideoMixerResource\n");

	//Init video mixer
	inited = mixer.Init(comp,size);

	//Exti
	return inited;
}

int VideoMixerResource::CreatePort(std::wstring &tag, int mosaicId)
{
	Log(">Create VideoMixerResourcePort\n");

	//SI no tamos iniciados pasamos
	if (!inited)
		return Error("Not inited\n");

	//Obtenemos el id
	int portId = maxId++;

	//Y el de video
	if (!mixer.CreateMixer(portId))
		return Error("Couldn't set video mixer\n");

	//Create the video port
	Port *port = new Port(tag);

	//Init encoder and decoder
	port->encoder.Init(mixer.GetInput(portId));
	port->decoder.Init(mixer.GetOutput(portId));

	//Init mixer id
	mixer.InitMixer(portId,mosaicId);

	//Lo insertamos en el map
	ports[portId] = port;

	Log("<CreateParticipant [%d]\n",portId);

	return portId;
}

int VideoMixerResource::SetPortCodec(int portId,VideoCodec::Type codec,int mode,int fps,int bitrate,int qMin, int qMax,int intraPeriod)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Video port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//Set codec
	port->encoder.SetCodec(codec,mode,fps,bitrate,qMin,qMax,intraPeriod);

	//Start
	return 1;
}

int VideoMixerResource::DeletePort(int portId)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Video port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//Remove it from list
	ports.erase(it);

	//End mixer id
	mixer.EndMixer(portId);

	//End encoder and decoder
	port->encoder.End();
	port->decoder.End();

	//End remove
	mixer.DeleteMixer(portId);

	//Delete object
	delete (port);

	//OK
	return 1;
}

int VideoMixerResource::End()
{
	//For each port
	for (Ports::iterator it = ports.begin(); it!= ports.end();++it)
	{
		//Get port
		Port *port = it->second;

		//End encoder and decoder
		port->encoder.End();
		port->decoder.End();

		//Delete object
		delete (port);
	}

	//Clear list
	ports.clear();

	//End mixer
	mixer.End();

	return 1;
}

Joinable* VideoMixerResource:: GetJoinable(int portId)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return (Joinable*)Error("Video port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//Return it
	return &port->encoder;
}

int VideoMixerResource::Attach(int portId,Joinable *join)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Video port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//OK
	return port->decoder.Attach(join);
}

int VideoMixerResource::Dettach(int portId)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Video port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//OK
	return port->decoder.Dettach();
}

int VideoMixerResource::CreateMosaic(Mosaic::Type comp,int size)
{
	return mixer.CreateMosaic(comp,size);
}

int VideoMixerResource::AddMosaicParticipant(int mosaicId,int portId)
{
	return mixer.AddMosaicParticipant(mosaicId,portId);
}

int VideoMixerResource::RemoveMosaicParticipant(int mosaicId,int portId)
{
	return mixer.RemoveMosaicParticipant(mosaicId,portId);
}

int VideoMixerResource::SetSlot(int mosaicId,int num,int id)
{
	return mixer.SetSlot(mosaicId,num,id);
}

int VideoMixerResource::SetCompositionType(int mosaicId,Mosaic::Type comp,int size)
{
	return mixer.SetCompositionType(mosaicId,comp,size);
}

int VideoMixerResource::SetOverlayPNG(int mosaicId,const char* overlay)
{
	return mixer.SetMosaicOverlayImage(mosaicId,overlay);
}

int VideoMixerResource::ResetOverlay(int mosaicId)
{
	return mixer.ResetMosaicOverlay(mosaicId);
}

int VideoMixerResource::DeleteMosaic(int mosaicId)
{
	return mixer.DeleteMosaic(mosaicId);
}
