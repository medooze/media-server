#include <tools.h>
#include "log.h"
#include <videomixer.h>
#include <pipevideoinput.h>
#include <pipevideooutput.h>
#include <set>
#include <functional>

typedef std::pair<int, DWORD> Pair;
typedef std::set<Pair, std::less<Pair>    > OrderedSetOfPairs;
typedef std::set<Pair, std::greater<Pair> > RevOrderedSetOfPairs;


DWORD VideoMixer::vadDefaultChangePeriod = 2000;
int VideoMixer::MosaicDefault = 0;
int VideoMixer::NoMosaic = -1;

void VideoMixer::SetVADDefaultChangePeriod(DWORD ms)
{
	//Set it
	vadDefaultChangePeriod = ms;
	//Log it
	Log("-VideoMixer VAD default change period set to %dms\n",vadDefaultChangePeriod);
}
/***********************
* VideoMixer
*	Constructord
************************/
VideoMixer::VideoMixer(const std::wstring &tag) :
	eventSource(tag),
	tag(tag)
{
	//Inciamos lso mutex y la condicion
	pthread_mutex_init(&mixVideoMutex,0);
	pthread_cond_init(&mixVideoCond,0);
}

/***********************
* ~VideoMixer
*	Destructor
************************/
VideoMixer::~VideoMixer()
{
	//Liberamos los mutex
	pthread_mutex_destroy(&mixVideoMutex);
	pthread_cond_destroy(&mixVideoCond);
}

/***********************
* startMixingVideo
*	Helper thread function
************************/
void * VideoMixer::startMixingVideo(void *par)
{
        Log("-MixVideoThread [%p]\n",pthread_self());

	//Obtenemos el parametro
	VideoMixer *vm = (VideoMixer *)par;

	//Bloqueamos las seï¿½ales
	blocksignals();

	//Ejecutamos
	vm->MixVideo();
	//Exit
	return NULL;
}

/************************
* MixVideo
*	Thread de mezclado de video
*************************/
int VideoMixer::MixVideo()
{
	struct timespec   ts;
	struct timeval    tp;
	int forceUpdate = 0;

	Log(">MixVideo\n");

	//Mientras estemos mezclando
	while(mixingVideo)
	{
		//LOck the mixing
		pthread_mutex_lock(&mixVideoMutex);

		//Everything is updated
		forceUpdate = 0;

		//Get now
		gettimeofday(&tp, NULL);

		//Calculate timeout
		calcAbsTimeout(&ts,&tp,100);

		//Wait for new images or timeout and adquire mutex on exit
		if (pthread_cond_timedwait(&mixVideoCond,&mixVideoMutex,&ts)==ETIMEDOUT)
		{

			//Force an update each 1/10 of second
			forceUpdate = 1;
			//Desbloqueamos
			pthread_mutex_unlock(&mixVideoMutex);
			//go to the begining
			continue;
		}
		
		//Desbloqueamos
		pthread_mutex_unlock(&mixVideoMutex);

		//Process 
		Process(forceUpdate,getTime());
	}

	Log("<MixVideo\n");
}


void VideoMixer::Process(bool forceUpdate, QWORD now)
{
	//Protegemos la lista
	lstVideosUse.WaitUnusedAndLock();

	//New version
	version++;

	//For each mosaic
	for (Mosaics::iterator itMosaic=mosaics.begin();itMosaic!=mosaics.end();++itMosaic)
	{

		//Get Mosaic
		Mosaic *mosaic = itMosaic->second;

		//Get number of slots
		int numSlots = mosaic->GetNumSlots();

		//Max vad value
		DWORD maxVAD = 0;

		//Get old speaker participant
		int oldVad = mosaic->GetVADParticipant();

		//Keep latest speaker if no one is talking
		int vadId = oldVad;

		//If VAD is set and we have the VAD proxy enabled do the "VAD thing"!
		//If there was no previous speaker or the vad slot is not blocked
		//If vad is not shown do nothing
		if (vadMode!=NoVAD && proxy && mosaic->IsVADShown() && (oldVad==0 || mosaic->GetVADBlockingTime()<=now))
		{
			//Update VAD info for each participant
			for (Videos::iterator it=lstVideos.begin();it!=lstVideos.end();++it)
			{
				//Get Id of participant
				int id = it->first;
				//If it is in the mosaic
				if (mosaic->HasParticipant(id))
				{
					//Get vad value for participant
					DWORD vad = proxy->GetVAD(id);
					UltraDebug("-VAD %d:%u\n",id,vad);
					//Found the highest VAD participant but select at least one.
					if (vad>maxVAD || vadId==0)
					{
						//Store max vad value
						maxVAD = vad;
						//Store speaker participant
						vadId = id;
					}
				}
			}
			//Check if there is a different active speaker
			if (oldVad!=vadId)
			{
				//Do we need to hide it?
				bool hide = (vadMode==FullVAD);
				// set the VAD participant
				mosaic->SetVADParticipant(vadId,hide,now + vadDefaultChangePeriod*1000);
				//If there was a previous active spearkc and wein FULL vad
				if (oldVad && hide)
					//Set score of old particippant to last time on VAD slot
					mosaic->SetScore(oldVad,now);
				//Calculate participants again
				mosaic->CalculatePositions();
			}
		}

		//Old and new positions
		int* newPos = (int*) malloc(numSlots*sizeof(int));
		int* oldPos = (int*) malloc(numSlots*sizeof(int));

		//Get info from mosaic
		memcpy(oldPos,mosaic->GetOldPositions(),numSlots*sizeof(int));
		memcpy(newPos,mosaic->GetPositions(),numSlots*sizeof(int));

		//Reset the change status in the mosaic
		mosaic->Reset();

		//For each slot
		for (int i=0;i<numSlots;i++)
		{
			//Get participant for slot
			int partId = newPos[i];

			//Check if it has changed
			bool changed = (oldPos[i]!=partId);

			//If there is a participant in the slot
			if (partId)
			{
				//Find  it
				Videos::iterator it = lstVideos.find(partId);

				//Double check
				if (it==lstVideos.end())
				{
					//Error
					Error("-participant not found %d for slot %d,cleaning it\n",partId,i);
					//If it was not there previously
					if (changed)
						//Clean position
						mosaic->Clean(i,logo);
					//Next slot
					continue;
				}

				//If we are displaying names
				if (displayNames && !it->second->name.empty())
				{
					//Get properties depending on the speaking threshold
					Properties& properties = (speakingThreshold && proxy->GetVAD(partId)>speakingThreshold) ? overlaySpeaking : overlay;
					//Get heigth
					int height = properties.GetProperty("height",30);
					//Set name
					mosaic->RenderOverlayText(it->second->name,mosaic->GetLeft(i),mosaic->GetTop(i)+mosaic->GetHeight(i)-height,mosaic->GetWidth(i),height,properties);
				}

				//Get output
				PipeVideoOutput *output = it->second->output;
				
				//Lock it
				output->Lock();

				//If we've got a new frame or the participant image was not in slot yet
				if ((output && output->IsChanged(version)) || changed)
				{
					//Change mosaic
					mosaic->Update(i,output->GetFrame(),output->GetWidth(),output->GetHeight(),keepAspectRatio);

					//Check if debug is enabled
					if (vadMode!=NoVAD && proxy && Logger::IsDebugEnabled())
					{
						//Get vad
						DWORD vad = proxy->GetVAD(partId);
						//Set VU meter
						mosaic->DrawVUMeter(i,vad,48000);
					}
				}
				//Release it
				output->Unlock();
			} else if (changed) {
				//Clean position
				mosaic->Clean(i,logo);
			}
		}
		//Free mem
		free(oldPos);
		free(newPos);
	}

	//For each video
	for (Videos::iterator it=lstVideos.begin();it!=lstVideos.end();++it)
	{
		//Get Source
		VideoSource *source = it->second;
		//Get input
		PipeVideoInput *input = source->input;

		//Get mosaic
		Mosaic *mosaic = source->mosaic;

		//Si no ha cambiado el frame volvemos al principio
		if (input && mosaic && (source->refresh || mosaic->HasChanged() || forceUpdate))
			//Colocamos el frame
			input->SetFrame(mosaic->GetFrame(),mosaic->GetWidth(),mosaic->GetHeight());
		//Reset refresh 
		source->refresh = true;
	}
	
	//Reset overlays if displaying names
	if (displayNames) 
		//For each mosaic
		for (Mosaics::iterator itMosaic=mosaics.begin();itMosaic!=mosaics.end();++itMosaic)
			//FIX: Reset text overlay
			itMosaic->second->SetOverlayText();
	
	//Desprotege la lista
	lstVideosUse.Unlock();
}
/*******************************
 * CreateMosaic
 *	Create new mosaic in the conference
 **************************************/
int VideoMixer::CreateMosaic(Mosaic::Type comp, int size)
{
	Log(">Create mosaic\n");

	//Get the new id
	int mosaicId = maxMosaics++;

	//Create mosaic
	SetCompositionType(mosaicId, comp, size);

	Log("<Create mosaic  [id:%d]\n",mosaicId);

	//Return the new id
	return mosaicId;
}

/*******************************
 * SetMosaicOverlayImage
 *	Set an overlay image in mosaic
 **************************************/
int VideoMixer::SetMosaicOverlayImage(int mosaicId,const char* filename)
{
	Log("-SetMosaicOverlayImage [id:%d,\"%s\"]\n",mosaicId,filename);

	//Protegemos la lista
	lstVideosUse.IncUse();
	
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
	{
		//Unlock
		lstVideosUse.DecUse();
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);
	}

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//Set overlay
	int ret =  mosaic->SetOverlayPNG(filename);
	
	//Unlock
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);
	
	//Exit
	return ret;
}

/*******************************
 * ResetMosaicOverlay
 *	Set an overlay image in mosaic
 **************************************/
int VideoMixer::ResetMosaicOverlay(int mosaicId)
{
	Log("-ResetMosaicOverlay [id:%d]\n",mosaicId);

	//Protegemos la lista
	lstVideosUse.IncUse();
	
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
	{
		//Unlock
		lstVideosUse.DecUse();
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);
	}

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//REset overlay
	int ret =  mosaic->ResetOverlay();
	
	//Unlock
	lstVideosUse.DecUse();
	
	//Exit
	return ret;
}

int VideoMixer::GetMosaicPositions(int mosaicId,std::list<int> &positions)
{
	Log("-GetMosaicPositions [id:%d]\n",mosaicId);

	//Protegemos la lista
	lstVideosUse.IncUse();
	
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
	{
		//Unlock
		lstVideosUse.DecUse();
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);
	}

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//Get num slots
	DWORD numSlots = mosaic->GetNumSlots();

	//Get data
	int* mosaicPos = mosaic->GetPositions();

	//For each pos
	for (int i=0;i<numSlots;++i)
		//Add it
		positions.push_back(mosaicPos[i]);

	//Unlock
	lstVideosUse.DecUse();
	
	//Exit
	return numSlots;
}

/***********************
* Init
*	Inicializa el mezclado de video
************************/
int VideoMixer::Init(const Properties &properties)
{
	//Get properties
	Mosaic::Type comp	= (Mosaic::Type) properties.GetProperty("mosaics.default.compType"	, (int)Mosaic::mosaic2x2);
	int size		= properties.GetProperty("mosaics.default.size"				, CIF);
	const char *logoFile	= properties.GetProperty("logo"						, "logo.png");
	
	//Should we display names?
	displayNames = properties.GetProperty("displayNames", false);
	//Get threshold for switching to speaking mode
	speakingThreshold = properties.GetProperty("speakingThreshold",48000);
	
	//Get overlay children properties
	properties.GetChildren("overlay",overlay);
	overlay.GetChildren("speaking",overlaySpeaking);
		
	//Load file
	logo.Load(logoFile);

	//Create default misxer
	int id = CreateMosaic(comp,size);

	//Set default
	defaultMosaic = mosaics[id];

	//Set ini time
	ini = properties.GetProperty("ini",getTime());
	
	//Check if we are in online or offline mode
	if (properties.GetProperty("online",true))
	{
		// Estamos mzclando
		mixingVideo = true;
		//Y arrancamoe el thread
		createPriorityThread(&mixVideoThread,startMixingVideo,this,0);
	}

	return 1;
}

/***********************
* End
*	Termina el mezclado de video
************************/
int VideoMixer::End()
{
	Log(">End videomixer\n");

	//Borramos los videos
	Videos::iterator it;

	//Terminamos con la mezcla
	if (mixingVideo)
	{
		//Terminamos la mezcla
		mixingVideo = 0;

		//LOck the mixing
		pthread_mutex_lock(&mixVideoMutex);

		//Signal for new video
		pthread_cond_signal(&mixVideoCond);

		//UNlock mixing
		pthread_mutex_unlock(&mixVideoMutex);

		//Y esperamos
		pthread_join(mixVideoThread,NULL);
	}

	//Protegemos la lista
	lstVideosUse.WaitUnusedAndLock();

	//Recorremos la lista
	for (it =lstVideos.begin();it!=lstVideos.end();++it)
	{
		//Obtenemos el video source
		VideoSource *video = (*it).second;
		//Delete video stream
		delete video->input;
		delete video->output;
		delete video;
	}

	//Clean the list
	lstVideos.clear();

	//For each mosaic
	for (Mosaics::iterator it=mosaics.begin();it!=mosaics.end();++it)
	{
		//Get mosaic
		Mosaic *mosaic = it->second;
		//Delete the mosaic
		delete(mosaic);
	}

	//Clean list
	mosaics.clear();
	
	//Close logo
	logo.Close();

	//Desprotegemos la lista
	lstVideosUse.Unlock();

	Log("<End videomixer\n");

	return 1;
}

/***********************
* CreateMixer
*	Crea una nuevo source de video para mezclar
************************/
int VideoMixer::CreateMixer(int id,const std::wstring &name)
{
	Log(">CreateMixer video [%d]\n",id);

	//Block list
	lstVideosUse.WaitUnusedAndLock();

	//Miramos que si esta
	if (lstVideos.find(id)!=lstVideos.end())
	{
		//Desprotegemos la lista
		lstVideosUse.Unlock();
		//Exit
		return Error("Video sourecer already existed\n");
	}

	//Creamos el source
	VideoSource *video = new VideoSource(name);

	//POnemos el input y el output
	video->input  = new PipeVideoInput();
	video->output = new PipeVideoOutput(&mixVideoMutex,&mixVideoCond);
	//No mosaic yet
	video->mosaic = NULL;

	//Add to list
	lstVideos[id] = video;

	//Unlock
	lstVideosUse.Unlock();

	//Y salimos
	Log("<CreateMixer video\n");

	return true;
}

/***********************
* InitMixer
*	Inicializa un video
*************************/
int VideoMixer::InitMixer(int id,int mosaicId)
{
	Log(">Init mixer [id:%d,mosaic:%d]\n",id,mosaicId);

	//Protegemos la lista
	lstVideosUse.IncUse();

	//Buscamos el video source
	Videos::iterator it = lstVideos.find(id);

	//Si no esta
	if (it == lstVideos.end())
	{
		//Desprotegemos
		lstVideosUse.DecUse();
		//Salimos
		return Error("Mixer not found\n");
	}

	//Obtenemos el video source
	VideoSource *video = (*it).second;

	//INiciamos los pipes
	video->input->Init();
	video->output->Init();

	//Get the mosaic for the user
	Mosaics::iterator itMosaic = mosaics.find(mosaicId);

	//If found
	if (itMosaic!=mosaics.end())
		//Set mosaic
		video->mosaic = itMosaic->second;
	else
		//Send only participant
		Log("-No mosaic for participant found, will be send only.\n");

	//Desprotegemos
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	Log("<Init mixer [%d]\n",id);

	//Si esta devolvemos el input
	return true;
}

/***********************************
 * SetMixerMosaic
 *	Add a participant to be shown in a mosaic
 ************************************/
int VideoMixer::SetMixerMosaic(int id,int mosaicId)
{
	Log(">SetMixerMosaic [id:%d,mosaic:%d]\n",id,mosaicId);

	//Protegemos la lista
	lstVideosUse.IncUse();

	//Get the mosaic for the user
	Mosaics::iterator itMosaic = mosaics.find(mosaicId);

	//Get mosaic
	Mosaic *mosaic = NULL;

	//If found
	if (itMosaic!=mosaics.end())
		//Set it
		mosaic = itMosaic->second;
	else
		//Send only participant
		Log("-No mosaic for participant found, will be send only.\n");

	//Buscamos el video source
	Videos::iterator it = lstVideos.find(id);

	//Si no esta
	if (it == lstVideos.end())
	{
		//Desprotegemos
		lstVideosUse.DecUse();
		//Salimos
		return Error("Mixer not found\n");
	}

	//Obtenemos el video source
	VideoSource *video = (*it).second;

	//Set mosaic
	video->mosaic = mosaic;
	
	//Refresh it
	video->refresh = true;

	//Desprotegemos
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	Log("<SetMixerMosaic [%d]\n",id);

	//Si esta devolvemos el input
	return true;
}

int  VideoMixer::SetMixerName(int id,const std::wstring &name)
{
	Debug(">SetMixerName [id:%d,mosaic:%ls]\n",id,name.c_str());

	//Protegemos la lista
	lstVideosUse.IncUse();
	
	//Buscamos el video source
	Videos::iterator it = lstVideos.find(id);

	//Si no esta
	if (it == lstVideos.end())
	{
		//Desprotegemos
		lstVideosUse.DecUse();
		//Salimos
		return Error("Mixer not found\n");
	}

	//Obtenemos el video source
	VideoSource *video = (*it).second;

	//Set mosaic
	video->name = name;
	
	//Refresh it
	video->refresh = true;

	//Desprotegemos
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	Debug("<SetMixerName [%d]\n",id);

	//Si esta devolvemos el input
	return true;
}

/***********************************
 * AddMosaicParticipant
 *	Add a participant to be shown in a mosaic
 ************************************/
int VideoMixer::AddMosaicParticipant(int mosaicId, int partId)
{
	Log("-AddMosaicParticipant [mosaic:%d,partId:%d]\n",mosaicId,partId);

	//Block
	lstVideosUse.IncUse();

	//Get the mosaic for the user
	Mosaics::iterator itMosaic = mosaics.find(mosaicId);

	//If not found
	if (itMosaic==mosaics.end())
	{
		//Unlock
		lstVideosUse.DecUse();
		//Salimos
		return Error("Mosaic not found\n");
	}
	
	//Get mosaic
	Mosaic* mosaic = itMosaic->second;

	//Add participant to the mosaic with score so it is last
	mosaic->AddParticipant(partId,partId<ini ? ini-partId : 0);

	//Recalculate positions
	mosaic->CalculatePositions();

	//Dump positions
	DumpMosaic(mosaicId,mosaic);

	//Unblock
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	//Everything ok
	return 1;
}

/***********************************
 * RemoveMosaicParticipant
 *	Remove a participant to be shown in a mosaic
 ************************************/
int VideoMixer::RemoveMosaicParticipant(int mosaicId, int partId)
{
	Log("-RemoveMosaicParticipant [mosaic:%d,partId:%d]\n",mosaicId,partId);

	//Block
	lstVideosUse.IncUse();

	//Get the mosaic for the user
	Mosaics::iterator itMosaic = mosaics.find(mosaicId);

	//If not found
	if (itMosaic==mosaics.end())
	{
		//Unblock
		lstVideosUse.DecUse();
		//Salimos
		return Error("Mosaic not found\n");
	}

	//Get mosaic
	Mosaic* mosaic = itMosaic->second;
	
	//Remove participant to the mosaic
	mosaic->RemoveParticipant(partId);

	//Recalculate positions
	mosaic->CalculatePositions();

	//Dump positions
	DumpMosaic(mosaicId,mosaic);

	//Unblock
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	//Correct
	return 1;
}

/***********************
* EndMixer
*	Finaliza un video
*************************/
int VideoMixer::EndMixer(int id)
{
	Log(">Endmixer [id:%d]\n",id);

	//Protegemos la lista
	lstVideosUse.IncUse();

	//Buscamos el video source
	Videos::iterator it = lstVideos.find(id);

	//Si no esta
	if (it == lstVideos.end())
	{
		//Desprotegemos
		lstVideosUse.DecUse();
		//Salimos
		return Error("-VideoMixer not found\n");
	}

	//Obtenemos el video source
	VideoSource *video = (*it).second;

	//Terminamos
	video->input->End();
	video->output->End();

	//Unset mosaic
	video->mosaic = NULL;

	//If still mixing video
	if (mixingVideo)
	{
		//For all the mosaics
		for (Mosaics::iterator it = mosaics.begin(); it!=mosaics.end(); ++it)
		{
			//Get mosaic
			Mosaic *mosaic = it->second;
			//Check if it is in mosaic
			if (mosaic->HasParticipant(id))
			{
				//Remove participant to the mosaic
				mosaic->RemoveParticipant(id);
				//Recalculate positions
				mosaic->CalculatePositions();
				//Dump positions
				DumpMosaic(it->first,mosaic);
			}
		}
	}
	
	//Dec usage
	lstVideosUse.DecUse();

	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	Log("<Endmixer [id:%d]\n",id);

	//Si esta devolvemos el input
	return true;
}

/***********************
* DeleteMixer
*	Borra una fuente de video
************************/
int VideoMixer::DeleteMixer(int id)
{
	Log(">DeleteMixer video [%d]\n",id);

	//Protegemos la lista
	lstVideosUse.WaitUnusedAndLock();

	//Lo buscamos
	Videos::iterator it = lstVideos.find(id);

	//SI no ta
	if (it == lstVideos.end())
	{
		//Desprotegemos la lista
		lstVideosUse.Unlock();
		//Salimos
		return Error("-Video mixer not found\n");
	}

	//Obtenemos el video source
	VideoSource *video = (*it).second;

	//Lo quitamos de la lista
	lstVideos.erase(id);

	//Desprotegemos la lista
	lstVideosUse.Unlock();

	//SI esta borramos los objetos
	delete video->input;
	delete video->output;
	delete video;

	Log("<DeleteMixer video [%d]\n",id);

	return 0;
}

/***********************
* GetInput
*	Obtiene el input para un id
************************/
VideoInput* VideoMixer::GetInput(int id)
{
	//Protegemos la lista
	lstVideosUse.IncUse();

	//Buscamos el video source
	Videos::iterator it = lstVideos.find(id);

	//Obtenemos el input
	VideoInput *input = NULL;

	//Si esta
	if (it != lstVideos.end())
		input = (VideoInput*)it->second->input;

	//Desprotegemos
	lstVideosUse.DecUse();

	//Return input
	return input;
}

/***********************
* GetOutput
*	Termina el mezclado de video
************************/
VideoOutput* VideoMixer::GetOutput(int id)
{
	//Protegemos la lista
	lstVideosUse.IncUse();

	//Buscamos el video source
	Videos::iterator it = lstVideos.find(id);

	//Obtenemos el output
	VideoOutput *output = NULL;

	//Si esta
	if (it != lstVideos.end())
		output = (VideoOutput*)it->second->output;

	//Desprotegemos
	lstVideosUse.DecUse();

	//return output
	return output;
}

int VideoMixer::SetMosaicPadding(int mosaicId, int paddingTop, int paddingRight, int paddingBottom, int paddingLeft)
{
	
	Log("-VideoMixer::SetMosaicPaddingVideoMixer::SetMosaicPadding() [id:%d,padding:{%d,%d,%d,%d}]\n",mosaicId, paddingTop, paddingRight, paddingBottom, paddingLeft);

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		return Error("VideoMixer::SetMosaicPadding() Mosaic not found");
	
	//Set padding
	return it->second->SetPadding(paddingTop,paddingRight,paddingBottom,paddingLeft);
}

int VideoMixer::SetMosaicOverlayText(int mosaicId)
{
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		return Error("VideoMixer::SetMosaicPadding() Mosaic not found");
	
	//Set padding
	return it->second->SetOverlayText();
}

int VideoMixer::RenderMosaicOverlayText(int mosaicId,const std::wstring& text,DWORD x,DWORD y,DWORD width,DWORD height, const Properties &properties)
{
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		return Error("VideoMixer::RenderMosaicOverlayText() Mosaic not found");
	
	//Set padding
	return it->second->RenderOverlayText(text,x,y,width,height,properties);
}

int VideoMixer::RenderMosaicOverlayText(int mosaicId,const std::string& utf8,DWORD x,DWORD y,DWORD width,DWORD height, const Properties &properties)
{
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		return Error("VideoMixer::RenderMosaicOverlayText() Mosaic not found");
	
	//Set padding
	return it->second->RenderOverlayText(utf8,x,y,width,height,properties);
}
	
/**************************
* SetCompositionType
*    Pone el modo de mosaico
***************************/
int VideoMixer::SetCompositionType(int mosaicId,Mosaic::Type comp, int size)
{
	Log(">SetCompositionType [id:%d,comp:%d,size:%d]\n",mosaicId,comp,size);

	//Create new mosaic
	Mosaic *mosaic = Mosaic::CreateMosaic(comp,size);
	
	//Protegemos la lista
	lstVideosUse.WaitUnusedAndLock();

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it!=mosaics.end())
	{
		//Get the old mosaic
		Mosaic *oldMosaic = it->second;

		//Add all the participants
		for (Videos::iterator it=lstVideos.begin();it!=lstVideos.end();++it)
		{
			//Get id
			DWORD partId = it->first;
			//Check if it was in the mosaic
			if (oldMosaic->HasParticipant(partId))
				//Add participant to mosaic
				mosaic->AddParticipant(partId,oldMosaic->GetScore(partId));
			//Check mosaic
			if (it->second->mosaic==oldMosaic)
				//Update to new one
				it->second->mosaic=mosaic;
		}

		//Set old slots
		mosaic->SetSlots(oldMosaic->GetSlots(),oldMosaic->GetNumSlots());

		//Set vad
		mosaic->SetVADParticipant(mosaic->GetVADParticipant(),(vadMode==FullVAD),mosaic->GetVADBlockingTime());

		//IF it is the defualt one
		if (oldMosaic==defaultMosaic)
			//Set new one as defautl
			defaultMosaic = mosaic;

		//Delete old one
		delete(oldMosaic);
	}

	//Recalculate positions
	mosaic->CalculatePositions();

	//Dump positions
	DumpMosaic(mosaicId,mosaic);

	//And in the list
	mosaics[mosaicId] = mosaic;

	//Unlock (Could this be done earlier??)
	lstVideosUse.Unlock();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	Log("<SetCompositionType\n");

	return 1;
}

/************************
/* SetSlot
*	Set slot participant
*************************/
int VideoMixer::SetSlot(int mosaicId,int num,int id)
{
	Log(">SetSlot [mosaicId:%d,num:%d,id:%d]\n",mosaicId,num,id);

	//Protegemos la lista
	lstVideosUse.IncUse();

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
	{
		//Unblock
		lstVideosUse.DecUse();
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);
	}

	//Get the  mosaic
	Mosaic *mosaic = it->second;

	//If it is a participant
	if (id>0)
	{
		//Get old position
		int old = mosaic->GetParticipantSlot(id);
		//If was fixed
		if (old!=Mosaic::PositionNotFound && old!=Mosaic::PositionNotShown)
			//Free it
			mosaic->SetSlot(old,Mosaic::SlotFree);
	}

	//Set it in the mosaic
	mosaic->SetSlot(num,id);

	//Recalculate positions
	mosaic->CalculatePositions();

	//Dump positions
	DumpMosaic(mosaicId,mosaic);

	//Desprotegemos la lista
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);
	
	Log("<SetSlot\n");

	return 1;
}


/************************
/* ResetSlots
*	Free all slots
*************************/
int VideoMixer::ResetSlots(int mosaicId)
{
	Log(">ResetSlots [mosaicId:%d]\n",mosaicId);

	//Protegemos la lista
	lstVideosUse.IncUse();

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
	{
		//Unblock
		lstVideosUse.DecUse();
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);
	}

	//Get the  mosaic
	Mosaic *mosaic = it->second;

	//For each slot
	for (int i=0; i<mosaic->GetNumSlots();++i)
		//If it was not VAD
		if ( mosaic->GetSlots()[i]!=Mosaic::SlotVAD)
			//Free it
			mosaic->SetSlot(i,Mosaic::SlotFree);

	//Recalculate positions
	mosaic->CalculatePositions();

	//Dump positions
	DumpMosaic(mosaicId,mosaic);

	//Desprotegemos la lista
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//UNlock mixing
	pthread_mutex_unlock(&mixVideoMutex);

	Log("<SetSlot\n");

	return 1;
}

int VideoMixer::DeleteMosaic(int mosaicId)
{
	Log("-Delete mosaic [id;%d]\n",mosaicId);
	
	//Blcok
	lstVideosUse.WaitUnusedAndLock();

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
	{
		//Unlock
		lstVideosUse.Unlock();
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);
	}

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//For each video
	for (Videos::iterator itv = lstVideos.begin(); itv!= lstVideos.end(); ++itv)
	{
		//Check it it has dis mosaic
		if (itv->second->mosaic == mosaic)
			//Set to null
			itv->second->mosaic = NULL;
	}

	//Remove mosaic
	mosaics.erase(it);

	//Blcok
	lstVideosUse.Unlock();

	//Delete mosaic
	delete(mosaic);

	//Exit
	return 1;
}

void VideoMixer::SetVADProxy(VADProxy* proxy)
{
	//Lock
	lstVideosUse.IncUse();
	//Set it
	this->proxy = proxy;
	//Unlock
	lstVideosUse.DecUse();
}

void VideoMixer::SetVADMode(VADMode vadMode)
{
	Log("-SetVadMode [%d]\n", vadMode);
	//Set it
	this->vadMode = vadMode;
}

void VideoMixer::SetKeepAspectRatio(bool keepAspectRatio)
{
	Log("-SetKeepAspectRatio [%d]\n", keepAspectRatio);
	//Set it
	this->keepAspectRatio = keepAspectRatio;
}

int VideoMixer::DumpMosaic(DWORD id,Mosaic* mosaic)
{
	char p[16];
	char line1[1024];
	char line2[1024];

	//Empty
	*line1=0;
	*line2=0;

	//Get num slots
	DWORD numSlots = mosaic->GetNumSlots();

	//Get data
	int* mosaicSlots = mosaic->GetSlots();
	int* mosaicPos   = mosaic->GetPositions();

	//Create string from array
	for (int i=0;i<numSlots;++i)
	{
		if (i)
		{
			strcat(line1,",");
			strcat(line2,",");
		}
		sprintf(p,"%.4d",mosaicSlots[i]);
		strcat(line1,p);
		sprintf(p,"%.4d",mosaicPos[i]);
		strcat(line2,p);
	}

	//Log
	Log("-MosaicSlots %d [%s]\n",id,line1);
	Log("-MosaicPos   %d [%s]\n",id,line2);

	const Mosaic::Participants& participants = mosaic->GetParticipants();
	Mosaic::Participants::const_iterator it = participants.begin();

	while(it!=participants.end())
	{
		Log("-MosaicPart %d score:%llu isFixed:%d\n",it->second->GetId(),it->second->GetScore(),it->second->GetIsFixed());
		it++;
	}	
	const Mosaic::ParticipantsOrder& order =  mosaic->GetParticipantsOrder();

	Mosaic::ParticipantsOrder::const_iterator ite = order.begin();

	while(ite!=order.end())
	{
		Log("-MosaicOrder %d\n",(*ite)->GetId());
		ite++;
	}	
	//Send event
	eventSource.SendEvent("mosaic","{id:%d,type:%d,slots:[%s],pos:[%s]}",id,mosaic->GetType(),line1,line2);

	//OK
	return 1;
}
