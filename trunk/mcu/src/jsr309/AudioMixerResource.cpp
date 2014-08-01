#include "log.h"
#include "AudioMixerResource.h"

AudioMixerResource::AudioMixerResource(std::wstring &tag)
{
	//Store tag
	this->tag = tag;

	//Not inited
	inited = false;

	//Init part counter
	maxId = 1;
}

AudioMixerResource::~AudioMixerResource()
{
	//Check if ended properly
	if (inited)
		//End!!
		End();
}

int AudioMixerResource::Init()
{

	Log("-Init AudioMixerResource\n");

	//Init audio mixer without vad
	inited = mixer.Init(Properties());

	//Exti
	return inited;
}

int AudioMixerResource::CreatePort(std::wstring &tag)
{
	Log(">Create AudioMixerResourcePort\n");

	//SI no tamos iniciados pasamos
	if (!inited)
		return Error("Not inited\n");

	//Obtenemos el id
	int portId = maxId++;

	//Y el de audio
	if (!mixer.CreateMixer(portId))
		return Error("Couldn't set audio mixer\n");

	//Create the audio port
	Port *port = new Port(tag);

	//Init encoder and decoder
	port->encoder.Init(mixer.GetInput(portId));
	port->decoder.Init(mixer.GetOutput(portId));

	//Init mixer id
	mixer.InitMixer(portId,0);

	//Lo insertamos en el map
	ports[portId] = port;

	Log("<CreateParticipant [%d]\n",portId);

	return portId;
}

int AudioMixerResource::SetPortCodec(int portId,AudioCodec::Type codec)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Audio port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//Set codec
	port->encoder.SetCodec(codec);
	
	//Start
	return 1;
}

int AudioMixerResource::DeletePort(int portId)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Audio port not found\n");

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

int AudioMixerResource::End()
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

Joinable* AudioMixerResource:: GetJoinable(int portId)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
	{
		//Error
		Error("Audio port not found\n");
		//Exit
		return NULL;
	}

	//LO obtenemos
	Port *port = it->second;

	//Return it
	return &port->encoder;
}

int AudioMixerResource::Attach(int portId,Joinable *join)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Audio port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//Init
	//OK
	return port->decoder.Attach(join);
}

int AudioMixerResource::Dettach(int portId)
{
	//Find port
	Ports::iterator it = ports.find(portId);

	//Check present
	if (it == ports.end())
		//Error
		return Error("Audio port not found\n");

	//LO obtenemos
	Port *port = it->second;

	//OK
	return port->decoder.Dettach();
}
