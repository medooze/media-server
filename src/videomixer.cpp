#include <tools.h>
#include "log.h"
#include <videomixer.h>
#include <pipevideoinput.h>
#include <pipevideooutput.h>

/***********************
* VideoMixer
*	Constructor
************************/
VideoMixer::VideoMixer()
{
	//Incializamos a cero
	defaultMosaic	= NULL;

	//No mosaics
	maxMosaics = 0;

	//No proxy
	proxy = NULL;
	
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
        Log("-MixVideoThread [%d]\n",getpid());

	//Obtenemos el parametro
	VideoMixer *vm = (VideoMixer *)par;
	
	//Bloqueamos las se�ales
	blocksignals();

	//Ejecutamos
	pthread_exit((void *)vm->MixVideo());
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
	DWORD version = 0;

	//Video Iterator
	Videos::iterator it;
	Mosaics::iterator itMosaic;

	Log(">MixVideo\n");

	//Mientras estemos mezclando
	while(mixingVideo)
	{
		//Protegemos la lista
		lstVideosUse.WaitUnusedAndLock();

		//For each video
		for (it=lstVideos.begin();it!=lstVideos.end();++it)
		{
			//Get input
			PipeVideoInput *input = it->second->input;

			//Get mosaic
			Mosaic *mosaic = it->second->mosaic;

			//Si no ha cambiado el frame volvemos al principio
			if (input && mosaic && (mosaic->HasChanged() || forceUpdate))
				//Colocamos el frame
				input->SetFrame(mosaic->GetFrame(),mosaic->GetWidth(),mosaic->GetHeight());
		}

		//Desprotege la lista
		lstVideosUse.Unlock();
		
		//For each mosaic
		for (itMosaic=mosaics.begin();itMosaic!=mosaics.end();++itMosaic)
			//Reset it
			itMosaic->second->Reset();

		//LOck the mixing
		pthread_mutex_lock(&mixVideoMutex);
		
		//Everything is updated
		forceUpdate = 0;

		//Get now
		gettimeofday(&tp, NULL);

		//Calculate timeout
		calcAbsTimeout(&ts,&tp,50);

		//Wait for new images or timeout and adquire mutex on exit
		if (pthread_cond_timedwait(&mixVideoCond,&mixVideoMutex,&ts)==ETIMEDOUT)
		{

			//Force an update each second
			forceUpdate = 1;
			//Desbloqueamos
			pthread_mutex_unlock(&mixVideoMutex);
			//go to the begining
			continue;
		}

		//Protegemos la lista
		lstVideosUse.WaitUnusedAndLock();

		//New version
		version++;

		//For each mosaic
		for (itMosaic=mosaics.begin();itMosaic!=mosaics.end();++itMosaic)
		{
			//Calculate max vad
			DWORD maxVAD = 0;
			//No maximum vad
			int vadId = -1;

			//Get Mosaic
			Mosaic *mosaic = itMosaic->second;

			//Nos recorremos los videos
			for (it=lstVideos.begin();it!=lstVideos.end();++it)
			{
				//Get Id
				int id = it->first;

				//Get output
				PipeVideoOutput *output = it->second->output;

				//Get position
				int pos = mosaic->GetPosition(id);

				//If we've got a new frame on source and it is visible
				if (output && output->IsChanged(version) && pos>=0)
					//Change mosaic
					mosaic->Update(pos,output->GetFrame(),output->GetWidth(),output->GetHeight());

				//Check it is on the mosaic and it is vad
				if (pos!=-2 && proxy)
				{
					//Get vad
					DWORD vad = proxy->GetVAD(id);
					//Check if position is fixed
					bool isFixed = mosaic->IsFixed(pos);
					//Check if it the maximun and the position is not fixed
					if (vad>maxVAD && !isFixed)
					{
						//Store max vad value
						maxVAD = vad;
						//Store speaker participant
						vadId = id;
					}
#ifdef MCUDEBUG
					//Check pos
					if (pos>=0)
						//Set VU meter
						mosaic->DrawVUMeter(pos,vad,48000);
#endif
				}
			}

			//Check vad is enabled
			if (proxy)
			{
				//Get posistion for VAD
				int pos = mosaic->GetVADPosition();
				
				//Check which if it is shown
				if (pos>=0)
				{
					//Check if it is a active speaked
					if (maxVAD>0)
					{
						//Get participant
						it = lstVideos.find(vadId);
						//If it is found
						if (it!=lstVideos.end())
						{
							//Get output
							PipeVideoOutput *output = it->second->output;
							//Change mosaic
							mosaic->Update(pos,output->GetFrame(),output->GetWidth(),output->GetHeight());
						}
					} else {
						//Update with logo
						mosaic->Update(pos,logo.GetFrame(),logo.GetWidth(),logo.GetHeight());
					}
				}
			}
		}

		//Desprotege la lista
		lstVideosUse.Unlock();

		//Desbloqueamos
		pthread_mutex_unlock(&mixVideoMutex);
	}

	Log("<MixVideo\n");
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

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//Exit
	return mosaic->SetOverlayPNG(filename);
}

/*******************************
 * ResetMosaicOverlay
 *	Set an overlay image in mosaic
 **************************************/
int VideoMixer::ResetMosaicOverlay(int mosaicId)
{
	Log("-ResetMosaicOverlay [id:%d]\n",mosaicId);
	
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//Exit
	return mosaic->ResetOverlay();
}

/***********************
* Init
*	Inicializa el mezclado de video
************************/
int VideoMixer::Init(Mosaic::Type comp,int size)
{
	//Allocamos para el logo
	logo.Load("logo.png");

	//Create default misxer
	int id = CreateMosaic(comp,size);

	//Set default
	defaultMosaic = mosaics[id];
	
	// Estamos mzclando
	mixingVideo = true;

	//Y arrancamoe el thread
	createPriorityThread(&mixVideoThread,startMixingVideo,this,0);

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

		//Se�alamos la condicion
		pthread_cond_signal(&mixVideoCond);

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
	
	//Desprotegemos la lista
	lstVideosUse.Unlock();

	Log("<End videomixer\n");
	
	return 1;
}

/***********************
* CreateMixer
*	Crea una nuevo source de video para mezclar
************************/
int VideoMixer::CreateMixer(int id)
{
	Log(">CreateMixer video [%d]\n",id);

	//Protegemos la lista
	lstVideosUse.WaitUnusedAndLock();

	//Miramos que si esta
	if (lstVideos.find(id)!=lstVideos.end())
	{
		//Desprotegemos la lista
		lstVideosUse.Unlock();
		return Error("Video sourecer already existed\n");
	}

	//Creamos el source
	VideoSource *video = new VideoSource();

	//POnemos el input y el output
	video->input  = new PipeVideoInput();
	video->output = new PipeVideoOutput(&mixVideoMutex,&mixVideoCond);
	//No mosaic yet
	video->mosaic = NULL;

	//Y lo a�adimos a la lista
	lstVideos[id] = video;

	//Desprotegemos la lista
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
	video->mosaic = mosaic;

	//Desprotegemos
	lstVideosUse.DecUse();

	Log("<SetMixerMosaic [%d]\n",id);

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

	//Get the mosaic for the user
	Mosaics::iterator itMosaic = mosaics.find(mosaicId);

	//If not found
	if (itMosaic==mosaics.end())
		//Salimos
		return Error("Mosaic not found\n");
	
	//Add participant to the mosaic
	itMosaic->second->AddParticipant(partId);

	//Everything ok
	return 1;
}

/***********************************
 * RemoveMosaicParticipant
 *	Remove a participant to be shown in a mosaic
 ************************************/
int VideoMixer::RemoveMosaicParticipant(int mosaicId, int partId)
{
	Log(">-RemoveMosaicParticipant [mosaic:%d,partId:%d]\n",mosaicId,partId);

	//Get the mosaic for the user
	Mosaics::iterator itMosaic = mosaics.find(mosaicId);

	//If not found
	if (itMosaic==mosaics.end())
		//Salimos
		return Error("Mosaic not found\n");

	//Get mosaic
	Mosaic* mosaic = itMosaic->second;

	//Remove participant to the mosaic
	int pos = mosaic->RemoveParticipant(partId);

	//If was shown
	if (pos!=-1)
		//Update mosaic
		UpdateMosaic(mosaic);

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

	//Dec usage
	lstVideosUse.DecUse();
	
	//LOck the mixing
	pthread_mutex_lock(&mixVideoMutex);

	//If still mixing video
	if (mixingVideo)
	{
		//For all the mosaics
		for (Mosaics::iterator it = mosaics.begin(); it!=mosaics.end(); ++it)
		{
			//Get mosaic
			Mosaic *mosaic = it->second;
			//Remove particiapant ande get position for user
			int pos = mosaic->RemoveParticipant(id);
			Log("-Removed from mosaic [mosaicId:%d,pos:%d]\n",it->first,pos);
			//If was shown
			if (pos!=-1)
				//Update mosaic
				UpdateMosaic(mosaic);
		}
	}

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
		return Error("Video mixer not found\n");
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
		input = (VideoInput*)(*it).second->input;

	//Desprotegemos
	lstVideosUse.DecUse();

	//Si esta devolvemos el input
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
		output = (VideoOutput*)(*it).second->output;

	//Desprotegemos
	lstVideosUse.DecUse();

	//Si esta devolvemos el input
	return (VideoOutput*)(*it).second->output;
}

/**************************
* SetCompositionType
*	Pone el modo de mosaico
***************************/
int VideoMixer::SetCompositionType(int mosaicId,Mosaic::Type comp, int size)
{
	Log(">SetCompositionType [id:%d,comp:%d,size:%d]\n",mosaicId,comp,size);

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//No mosaic
	Mosaic *oldMosaic = NULL;

	//Check if we have found it
	if (it!=mosaics.end())
		//Get the old mosaic
		oldMosaic = it->second;

	//New mosaic
	Mosaic *mosaic = Mosaic::CreateMosaic(comp,size);

	//Protegemos la lista
	lstVideosUse.WaitUnusedAndLock();

	//If we had a previus mosaic
	if (oldMosaic)
	{
		//Add all the participants
		for (Videos::iterator it=lstVideos.begin();it!=lstVideos.end();++it)
		{
			//Get id
			DWORD partId = it->first;
			//Check if it was in the mosaic
			if (oldMosaic->HasParticipant(partId))
				//Add participant to mosaic
				mosaic->AddParticipant(partId);
			//Check mosaic
			if (it->second->mosaic==oldMosaic)
				//Update to new one
				it->second->mosaic=mosaic;
		}

		//Set old slots
		mosaic->SetSlots(oldMosaic->GetSlots(),oldMosaic->GetNumSlots());

		//IF it is the defualt one
		if (oldMosaic==defaultMosaic)
			//Set new one as defautl
			defaultMosaic = mosaic;
		
		//Delete old one
		delete(oldMosaic);
	}

	//Recalculate positions
	mosaic->CalculatePositions();

	//Update it
	UpdateMosaic(mosaic);

	//And in the list
	mosaics[mosaicId] = mosaic;

	//Signal for new video
	pthread_cond_signal(&mixVideoCond);

	//Desprotegemos la lista
	lstVideosUse.Unlock();

	Log("<SetCompositionType\n");

	return 1;
}

/************************
* CalculatePosition
*	Calculate positions for participants
*************************/
int VideoMixer::UpdateMosaic(Mosaic* mosaic)
{
	Log(">Updating mosaic\n");

	//Get positions
	int *positions = mosaic->GetPositions();

	//Get number of slots
	int numSlots = mosaic->GetNumSlots();
	
	//For each one
	for (int i=0;i<numSlots;i++)
	{
		//If it is has a participant
		if (positions[i]>0)
		{
			//Find video
			Videos::iterator it = lstVideos.find(positions[i]);
			//Check we have found it
			if (it!=lstVideos.end())
			{
				//Get output
				PipeVideoOutput *output = it->second->output;
				//Update slot
				mosaic->Update(i,output->GetFrame(),output->GetWidth(),output->GetHeight());
			} else {
				//Update with logo
				mosaic->Update(i,logo.GetFrame(),logo.GetWidth(),logo.GetHeight());
			}
		} else {
			//Update with logo
			mosaic->Update(i,logo.GetFrame(),logo.GetWidth(),logo.GetHeight());
		}
	}

	Log("<Updated mosaic\n");
}

/************************
* SetSlot
*	Set slot participant
*************************/
int VideoMixer::SetSlot(int mosaicId,int num,int id)
{
	Log(">SetSlot [mosaicId:%d,num:%d,id:%d]\n",mosaicId,num,id);

	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//If it does not have mosaic
	if (!mosaic)
		//Exit
		return Error("Null mosaic");

	//Protegemos la lista
	lstVideosUse.WaitUnusedAndLock();

	//Get position
	int pos = mosaic->GetPosition(id);

	//If it was shown
	if (pos>=0)
		//Update with logo
		mosaic->Update(pos,logo.GetFrame(),logo.GetWidth(),logo.GetHeight());

	//Set it in the mosaic
	mosaic->SetSlot(num,id);

	//Calculate positions
	mosaic->CalculatePositions();

	//Update it
	UpdateMosaic(mosaic);

	//Desprotegemos la lista
	lstVideosUse.Unlock();

	Log("<SetSlot\n");

	return 1;
}

int VideoMixer::DeleteMosaic(int mosaicId)
{
	//Get mosaic from id
	Mosaics::iterator it = mosaics.find(mosaicId);

	//Check if we have found it
	if (it==mosaics.end())
		//error
		return Error("Mosaic not found [id:%d]\n",mosaicId);

	//Get the old mosaic
	Mosaic *mosaic = it->second;

	//Blcok
	lstVideosUse.IncUse();

	//For each video
	for (Videos::iterator itv = lstVideos.begin(); itv!= lstVideos.end(); ++itv)
	{
		//Check it it has dis mosaic
		if (itv->second->mosaic == mosaic)
			//Set to null
			itv->second->mosaic = NULL;
	}

	//Blcok
	lstVideosUse.DecUse();

	//Remove mosaic
	mosaics.erase(it);

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