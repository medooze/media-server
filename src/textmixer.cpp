#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <tools.h>
#include <wchar.h>
#include "log.h"
#include "textmixer.h"
#include "pipetextinput.h"
#include "pipetextoutput.h"
#include "wait.h"


/***********************
* TextMixer
*	Constructor
************************/
TextMixer::TextMixer()
{
	//Creamos  los mutex
	pthread_mutex_init(&mixTextMutex,0);
}

/***********************
* ~TextMixer
*	Destructor
************************/
TextMixer::~TextMixer()
{
	//Liberamos los mutex
	pthread_mutex_destroy(&mixTextMutex);
}

/***********************************
* startMixingText
*	Crea el thread de mezclado de text
************************************/
void *TextMixer::startMixingText(void *par)
{
	Log("-MixTextThread [%p]\n",pthread_self());

	//Obtenemos el parametro
	TextMixer *am = (TextMixer *)par;

	//Bloqueamos las se�ales
	blocksignals();
	
	//Ejecutamos
	am->MixText();
	//Exit
	return NULL;
}

/***********************************
* MixText
*	Mezcla los texts
************************************/
int TextMixer::MixText()
{
	wchar_t buffer[1024];
	DWORD size=1024;

	Log(">MixText\n");

	//Lock list of text mixers
	lstTextsUse.WaitUnusedAndLock();
		
	//Mientras estemos mezclando
	while(mixingText)
	{
		//Send to all participants
		for (TextSources::iterator it=sources.begin();it!=sources.end();++it)
		{
			//Get text source
			TextSource *text = it->second;

			//Check if it has somethin in the queue
			if (text->output->Length())
			{
				//Read input
				DWORD len = text->output->ReadText(buffer,size);
				//Add to all workers
				for (TextWorkers::iterator w=workers.begin();w!=workers.end();++w)
				{
					//Get worker
					TextMixerWorker *worker = (*w);
					//Check it is not the ixer worker
					if (text->worker!=worker)
						//Write it
						worker->WriteText(text->id,buffer,len);
				}
			}
		}

		//Send private texts
		for (TextPrivates::iterator it=privates.begin();it!=privates.end();++it)
		{
			//Get text source
			TextPrivate *priv = it->second;

			//Check if it has somethin in the queue
			if (priv->output->Length())
			{
				//Read input
				DWORD len = priv->output->ReadText(buffer,size);
				//Get private worked
				//Add to all workers
				TextSources::iterator its = sources.find(priv->to);
				//If it setill exist
				if (its!=sources.end())
					//Write it
					its->second->worker->WriteText(priv->id,buffer,len);
			}
		}

		//Know for each worker
		for (TextWorkers::iterator w=workers.begin();w!=workers.end();++w)
			//Process it
			(*w)->ProcessText();
		
		//Un lock
		lstTextsUse.Unlock();
	
		//Check if we are canceled
		cancel.WaitSignal(200);
		
		//Lock list of text mixers
		lstTextsUse.WaitUnusedAndLock();
	}
	
	//Know for each worker
	for (TextWorkers::iterator w=workers.begin();w!=workers.end();++w)
		//Flush any text in the queue
		(*w)->FlushText();
	
	//Un lock
	lstTextsUse.Unlock();

	//Logeamos
	Log("<MixText\n");

	return 1;
}


/***********************
* Init
*	Inicializa el mezclado de text
************************/
int TextMixer::Init()
{
	// Estamos mzclando
	mixingText = true;

	//Y arrancamoe el thread
	createPriorityThread(&mixTextThread,startMixingText,this,0);

	return 1;
}

/***********************
* End
*	Termina el mezclado de text
************************/
int TextMixer::End()
{
	Log(">End textmixer\n");

	//Borramos los texts
	
	//Terminamos con la mezcla
	if (mixingText)
	{
		//Terminamos la mezcla
		mixingText = 0;
		
		//Stop waiting
		cancel.Signal();

		//Y esperamos
		pthread_join(mixTextThread,NULL);
	}
	
	//Lock
	lstTextsUse.WaitUnusedAndLock();

	//Get first
	TextSources::iterator it = sources.begin();
	
	//Recorremos la lista
	while (it!=sources.end())
	{
		//Get text
		TextSource *text = it->second;
		
		//delete from list and move to next
		sources.erase(it++);
		
		//Delete objects
		delete text->input;
		delete text->output;
		delete text->worker;
		delete text;
	}
	
	//Desprotegemos la lista
	lstTextsUse.Unlock();

	Log("<End textmixer\n");
	
	return 1;
}

/***********************
* CreateMixer
*	Crea una nuevo source de text para mezclar
************************/
int TextMixer::CreateMixer(int id,const std::wstring &name)
{
	Log(">CreateMixer text [%d]\n",id);

	//Protegemos la lista
	lstTextsUse.WaitUnusedAndLock();

	//Miramos que si esta
	if (sources.find(id)!=sources.end())
	{
		//Desprotegemos la lista
		lstTextsUse.Unlock();
		return Error("Text sourecer already existed\n");
	}

	//Creamos el source
	TextSource *text = new TextSource();

	//Set id
	text->id = id;

	//POnemos el input y el output
	text->input  = new PipeTextInput();
	text->output = new PipeTextOutput();
	//Create the worker
	text->worker = new TextMixerWorker();

	//Set name
	text->name = name;

	//Add source to the list
	sources[id] = text;
	
	Log("-Text [%d,%ls]\n",text->id,text->name.c_str());

	//Desprotegemos la lista
	lstTextsUse.Unlock();

	//Y salimos
	Log("<CreateMixer text\n");

	return true;
}

/***********************
* InitMixer
*	Inicializa un text
*************************/
int TextMixer::InitMixer(int id)
{
	Log(">Init mixer [%d]\n",id);

	//Protegemos la lista
	lstTextsUse.IncUse();

	//Buscamos el text source
	TextSources::iterator it = sources.find(id);

	//Si no esta	
	if (it == sources.end())
	{
		//Desprotegemos
		lstTextsUse.DecUse();
		//Salimos
		return Error("Mixer not found\n");
	}

	//Obtenemos el text source
	TextSource *text = it->second;

	//INiciamos los pipes
	text->input->Init();
	text->output->Init();

	//Init worker
	text->worker->Init();

	//Set participant as reader for the worker
	text->worker->AddReader(id,text->input);

	Log("-Text [%d,%ls]\n",text->id,text->name.c_str());

	//Add as writter to all the other participants
	for (TextWorkers::iterator it=workers.begin();it!=workers.end();++it)
		//Add writter
		(*it)->AddWritter(text->id,text->name,true);

	//Set all other participants as writters for the user
	for (TextSources::iterator it=sources.begin();it!=sources.end();++it)
	{
		//Get source
		TextSource *source = it->second;
		Log("[%d,%d]\n",text->id,source->id);
		//Check if it is us
		if (source->id!=text->id)
			//Add writer
			text->worker->AddWritter(source->id,source->name,true);
	}

	//Add the worker to the list
	workers.insert(text->worker);

	//Desprotegemos
	lstTextsUse.DecUse();

	Log("<Init mixer [%d]\n",id);

	//Si esta devolvemos el input
	return true;
}


/***********************
* EndMixer
*	Finaliza un text
*************************/
int TextMixer::EndMixer(int id)
{
	//Protegemos la lista
	lstTextsUse.IncUse();

	//Buscamos el text source
	TextSources::iterator it = sources.find(id);

	//Si no esta	
	if (it == sources.end())
	{
		//Desprotegemos
		lstTextsUse.DecUse();
		//Salimos
		return false;
	}

	//Obtenemos el text source
	TextSource *text = it->second;

	//Desprotegemos
	lstTextsUse.DecUse();

	//End the mixer
	text->worker->End();

	//Terminamos
	text->input->End();
	text->output->End();

	//Si esta devolvemos el input
	return true;
}

/***********************
* DeleteMixer
*	Borra una fuente de text
************************/
int TextMixer::DeleteMixer(int id)
{
	Log("-DeleteMixer text [%d]\n",id);

	//Protegemos la lista
	lstTextsUse.WaitUnusedAndLock();

	//Lo buscamos
	TextSources::iterator it = sources.find(id);

	//SI no ta
	if (it == sources.end())
	{
		//DDesprotegemos la lista
		lstTextsUse.Unlock();
		//Salimos
		return Error("Text source not found\n");
	}

	//Obtenemos el text source
	TextSource *text = it->second;

	//Remove from the workers
	workers.erase(text->worker);
	
	//Remove as writter to all the other participants
	for (TextWorkers::iterator it=workers.begin();it!=workers.end();++it)
		//Remove writter
		(*it)->RemoveWritter(text->id);

	//Remove from source list
	sources.erase(it);
	
	//Desprotegemos la lista
	lstTextsUse.Unlock();

	//SI esta borramos los objetos
	delete text->input;
	delete text->output;
	delete text->worker;
	delete text;

	return 0;
}

/***********************
* GetInput
*	Obtiene el input para un id
************************/
TextInput* TextMixer::GetInput(int id)
{
	//Protegemos la lista
	lstTextsUse.IncUse();

	//Buscamos el text source
	TextSources::iterator it = sources.find(id);

	//Obtenemos el input
	TextInput *input = NULL;

	//Si esta
	if (it != sources.end())
		input = (TextInput*)it->second->input;

	//Desprotegemos
	lstTextsUse.DecUse();

	//Si esta devolvemos el input
	return input;
}

/***********************
* GetOutput
*	Obtiene el output para un id
************************/
TextOutput* TextMixer::GetOutput(int id)
{
	//Protegemos la lista
	lstTextsUse.IncUse();

	//Buscamos el text source
	TextSources::iterator it = sources.find(id);

	//Obtenemos el output
	TextOutput *output = NULL;

	//Si esta	
	if (it != sources.end())
		//Store it
		output = it->second->output;

	//if still not found
	if (!output)
	{
		//Check if it is private
		TextPrivates::iterator it = privates.find(id);
		//If it exist
		if (it!=privates.end())
			//Store it
			output = it->second->output;
	}

	//Desprotegemos
	lstTextsUse.DecUse();

	//Si esta devolvemos el input
	return output;
}


/***********************
* CreatePrivate
*	Create a private text source for one participant
************************/
int TextMixer::CreatePrivate(int id,int to,std::wstring &name)
{
	Log(">CreatePrivate text [%d,%d]\n",id,to);

	//Protegemos la lista
	lstTextsUse.WaitUnusedAndLock();

	//Miramos que si esta
	if (privates.find(id)!=privates.end())
	{
		//Desprotegemos la lista
		lstTextsUse.Unlock();
		//Error
		return Error("Private sourecer already existed\n");
	}

	//Create the private text
	TextPrivate *priv = new TextPrivate();

	//Set id
	priv->id = id;
	//Set target mixer
	priv->to = to;
	//Create output
	priv->output = new PipeTextOutput();
	//Set name
	priv->name = name;

	//Add private the list
	privates[id] = priv;
	
	//Desprotegemos la lista
	lstTextsUse.Unlock();

	//Y salimos
	Log("<CreateMixer text\n");

	return true;
}

/***********************
* InitPrivate
*	Inicializa un text
*************************/
int TextMixer::InitPrivate(int id)
{
	Log(">Init private [%d]\n",id);

	//Protegemos la lista
	lstTextsUse.IncUse();

	//Buscamos el text source
	TextPrivates::iterator it = privates.find(id);

	//Si no esta
	if (it == privates.end())
	{
		//Desprotegemos
		lstTextsUse.DecUse();
		//Salimos
		return Error("Mixer not found\n");
	}

	//Obtenemos el text source
	TextPrivate *priv = it->second;

	//INiciamos los pipes
	priv->output->Init();

	//Find private target
	TextSources::iterator itSource=sources.find(priv->to);
	
	//if found
	if (itSource!=sources.end())
		//Add writer
		itSource->second->worker->AddWritter(id,priv->name,false);

	//Desprotegemos
	lstTextsUse.DecUse();

	Log("<Init private [%d]\n",id);

	//Si esta devolvemos el input
	return true;
}


/***********************
* EndPrivate
*	Finaliza un text
*************************/
int TextMixer::EndPrivate(int id)
{
	//Protegemos la lista
	lstTextsUse.IncUse();

	//Buscamos el text source
	TextPrivates::iterator it = privates.find(id);

	//Si no esta
	if (it == privates.end())
	{
		//Desprotegemos
		lstTextsUse.DecUse();
		//Salimos
		return false;
	}

	//Obtenemos el text source
	TextPrivate *priv = it->second;

	//Desprotegemos
	lstTextsUse.DecUse();
	
	//Terminamos
	priv->output->End();

	//Si esta devolvemos el input
	return true;;
}

/***********************
* DeletePrivate
*	Borra una fuente de text
************************/
int TextMixer::DeletePrivate(int id)
{
	Log("-DeletePrivate text [%d]\n",id);

	//Protegemos la lista
	lstTextsUse.WaitUnusedAndLock();

	///Buscamos el text source
	TextPrivates::iterator it = privates.find(id);

	//Si no esta
	if (it == privates.end())
	{
		//Desprotegemos
		lstTextsUse.Unlock();
		//Salimos
		return false;
	}

	//Obtenemos el text source
	TextPrivate *priv = it->second;

	//Lo quitamos de la lista
	privates.erase(it);
	
	//Find private target
	TextSources::iterator itSource=sources.find(priv->to);

	//If target found
	if (itSource!=sources.end())
		//Add writer
		itSource->second->worker->RemoveWritter(priv->id);

	//Desprotegemos la lista
	lstTextsUse.Unlock();

	//SI esta borramos los objetos
	delete priv->output;
	delete priv;

	return 0;
}
/***********************
* GetPrivateOutput
*	Obtiene el output para un id
************************/
TextOutput* TextMixer::GetPrivateOutput(int id)
{
	//Protegemos la lista
	lstTextsUse.IncUse();

	//Buscamos el text source
	TextPrivates::iterator it = privates.find(id);

	//Obtenemos el output
	TextOutput *output = NULL;

	//Si esta
	if (it != privates.end())
		output = it->second->output;

	//Desprotegemos
	lstTextsUse.DecUse();

	//Si esta devolvemos el input
	return output;
}
