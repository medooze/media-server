#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <set>
#include <list>
#include "textencoder.h"
#include "log.h"
#include "tools.h"
#include "text.h"


/**********************************
* TextEncoder
*	Constructor
***********************************/
TextEncoder::TextEncoder()
{
	//Not encoding
	encodingText=0;
	//Create mutex
	pthread_mutex_init(&mutex,0);
}

/*******************************
* ~TextEncoder
*	Destructor.
********************************/
TextEncoder::~TextEncoder()
{
	//If still running
	if (encodingText)
		//End
		End();
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

/***************************************
* Init
*	Inicializa los devices
***************************************/
int TextEncoder::Init(TextInput *input)
{
	Log(">Init text encoder\n");

	//Nos quedamos con los puntericos
	textInput  = input;

	//Y aun no estamos mandando nada
	encodingText=0;

	Log("<Init text encoder\n");

	return 1;
}

/***************************************
* startencodingText
*	Helper function
***************************************/
void * TextEncoder::startEncoding(void *par)
{
	TextEncoder *conf = (TextEncoder *)par;
	blocksignals();
	Log("Encoding text [%p]\n",pthread_self());
	conf->Encode();
	//Exit
	return NULL;
}


/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int TextEncoder::StartEncoding()
{
	Log(">Start encoding text\n");

	//Si estabamos mandando tenemos que parar
	if (encodingText)
		//paramos
		StopEncoding();

	encodingText=1;

	//Start thread
	createPriorityThread(&encodingTextThread,startEncoding,this,1);

	Log("<StartSending text [%d]\n",encodingText);

	return 1;
}
/***************************************
* End
*	Termina la conferencia activa
***************************************/
int TextEncoder::End()
{
	//If encoding
	if (encodingText)
		//Terminamos de enviar
		StopEncoding();

	return 1;
}


/***************************************
* StopEncoding
* 	Termina el envio
****************************************/
int TextEncoder::StopEncoding()
{
	Log(">StopEncoding Text [0x%x]\n",this);

	//Esperamos a que se cierren las threads de envio
	if (encodingText)
	{
		//paramos
		encodingText=0;

		Log("-Cancel text [0x%x]\n",textInput);

		//Cancel any pending grab
		textInput->Cancel();

		//Y esperamos
		pthread_join(encodingTextThread,NULL);
	}

	Log("<StopEncoding Text\n");

	return 1;
}

/*******************************************
* Encode
*	Capturamos el text y lo mandamos
*******************************************/
int TextEncoder::Encode()
{
	std::list<std::wstring> scroll;
	std::wstring line;

	//Mientras tengamos que capturar
	while(encodingText)
	{
		//Wait until there is a frame
		TextFrame *frame = textInput->GetFrame(0);

		//Check framce
		if (!frame)
			//next one
			continue;

		//If it has content
		if (frame->GetWLength())
		{
			//Get text
			std::wstring text = frame->GetWString();

			//Search each character
			for (int i=0;i<text.length();i++)
			{
				//Get char
				wchar_t ch = text.at(i);
				//Depending on the char
				switch (ch)
				{
					//MEssage delimiter
					case 0x0A:
					case 0x2028:
					case 0x2029:
						//Append an end line
						line.push_back(0x0A);
						//push the line
						scroll.push_back(line);
						//Empty it
						line.clear();
						break;
					//Check backspace
					case 0x08:
						//If not empty
						if (line.size())
							//Remove last
							line.erase(line.length()-1,1);
						break;
					//BOM
					case 0xFEFF:
						//Do nothing
						break;
					//Replacement
					case 0xFFFD:
						//Append .
						line.push_back('.');
						break;
					//Any other
					default:
						//Append it
						line.push_back(ch);
				}
			}

			//Create text to send
			std::wstring send;

			//Append lines in scroll
			for (std::list<std::wstring>::iterator it=scroll.begin();it!=scroll.end();it++)
				//Append
				send += (*it);

			//Append line also
			send += line;

			//Create new frame
			TextFrame f(frame->GetTimeStamp(),send);

			//Lock
			pthread_mutex_lock(&mutex);
			//For each listener
			for (Listeners::iterator it=listeners.begin(); it!=listeners.end(); ++it)
				//Call listener
				(*it)->onMediaFrame(f);
			//unlock
			pthread_mutex_unlock(&mutex);

			//Check number of lines in scroll
			if (scroll.size()>2)
				//Remove first
				scroll.pop_front();

			//Delete frame
			delete(frame);
		}
	}

	//Salimos
        Log("<Encode Text\n");
}

bool TextEncoder::AddListener(MediaFrame::Listener *listener)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Add to set
	listeners.insert(listener);

	//unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

bool TextEncoder::RemoveListener(MediaFrame::Listener *listener)
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Search
	Listeners::iterator it = listeners.find(listener);

	//If found
	if (it!=listeners.end())
		//End
		listeners.erase(it);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}
