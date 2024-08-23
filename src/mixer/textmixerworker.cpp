#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <tools.h>
#include <wchar.h>
#include "log.h"
#include "textmixerworker.h"
#include "fifo.h"


static wchar_t BOM[]			= {0xFEFF};
static wchar_t CRLF[]			= {0x0D,0x0A};
static wchar_t LF[]			= {0x0A};
static wchar_t INT[]			= {0x1B,61};
static wchar_t BEL[]			= {0x07};
static wchar_t BACKSPACE[]		= {0x08};
static wchar_t ERASURE_REPL[]		= {'X'};
static wchar_t LABEL_START[]		= {'['};
static wchar_t LABEL_END[]		= {']',' '};
static wchar_t SGR_START[]		= {0x009B};
static wchar_t SGR_END[]		= {0x006D};
static wchar_t SGR0[]			= {0x009B,0x00,0x006D};
static wchar_t LINE_SEPARATOR[]		= {0x2028};
static wchar_t PARAGRAPH_SEPARATOR[]	= {0x2029};

#define WLEN(str) 	sizeof(str)/sizeof(wchar_t)

TextMixerWorker::TextMixerWorker()
{
	//Set initial values
	currentWritter = NULL;
}

TextMixerWorker::~TextMixerWorker()
{
	//Delete all
	for(Writters::iterator it=writters.begin();it!=writters.end();++it)
		//Delete the writters
		delete(it->second);
	//Delete all
	for(Readers::iterator it=readers.begin();it!=readers.end();++it)
		//Delete the writters
		delete(it->second);
}

int TextMixerWorker::Init()
{
	return 1;
}

int TextMixerWorker::End()
{
	return 1;
}


int TextMixerWorker::AddWritter(DWORD id, const std::wstring name,bool realtime)
{
	Log("-AddWritter [id:%d,name:\"%ls\"]\n",id,name.c_str());
	//Check if we alredy have it
	if (writters.find(id)!=writters.end())
		//Error
		return 0;
	//Create
	writters[id] = new TextWritter(id,name,realtime);

	return 1;
}

int TextMixerWorker::WriteText(DWORD id,const wchar_t *data,DWORD size)
{
	//find it
	Writters::iterator it = writters.find(id);

	//Check if we have it
	if (it==writters.end())
		//Error
		return Error("Writter not found\n");

	//First ones
	DWORD ini = 0;
	DWORD len = 0;

	//Filter BOM
	for (int i=0;i<size;i++)
	{
		//Check if char is BOM
		if ((WLEN(BOM)<=size-i) && (wmemcmp(data+i,BOM,WLEN(BOM))==0))
		{
			//If we have length
			if (len)
				//Write until here
				it->second->textQueue.push(data+ini,len);
			//Set first
			ini = i+WLEN(BOM);
			//No data
			len = 0;
		} else {
			//Increase length
			len++;
		}
	}

	//If we have length
	if (len)
		//Write
		it->second->textQueue.push(data+ini,len);

	return it->second->textQueue.length();
}

int TextMixerWorker::RemoveWritter(DWORD id)
{
	Log("-RemoveWritter [id:%d]\n",id);
	//find it
	Writters::iterator it = writters.find(id);

	//Check if we have it
	if (it==writters.end())
		//Error
		return Error("Writter not found");
	
	//Get writter
	TextWritter *writter = it->second;

	//Check if it was the current participant
	if (currentWritter==writter)
	{
		//Send CRLF to all participants
		for (Readers::iterator it=readers.begin();it!=readers.end();++it)
		{
			//Get text source
			TextReader *reader = it->second;
			//Send CRLF
			reader->input->WriteText(CRLF,WLEN(CRLF));
			//Next is first
			reader->isFirst = 1;
		}
		//Change participant
		currentWritter = NULL;
	}

	//Delete it
	delete(it->second);
	//And remove
	writters.erase(it);
}

int TextMixerWorker::AddReader(DWORD id,PipeTextInput *input)
{
	Log("-AddReader [%d]\n",id);

	//Check if we alredy have it
	if (readers.find(id)!=readers.end())
		//Error
		return 0;
	
	//Send BOM
	input->WriteText(BOM,WLEN(BOM));
	
	//Create
	readers[id] = new TextReader(id,input);

	return 1;
}

int TextMixerWorker::RemoveReader(DWORD id)
{
	//find it
	Readers::iterator it = readers.find(id);

	//Check if we have it
	if (it==readers.end())
		//Error
		return 0;
	
	//Delete it
	delete(it->second);
	
	//And remove
	readers.erase(it);
}

int TextMixerWorker::ProcessText()
{
	wchar_t buffer[1024];
	wchar_t aux[1024];
	DWORD size=1024;

	QWORD maxWaitingTime = 0;
	TextWritter* nextCurrentParticipant = NULL;
	bool switchParticipant = false;
	bool searchMessageDelimiter = true;
	bool searchSentenceDelimiter = true;
	bool searchWordDelimiter = false;
	bool foundMessageDelimiter = false;
	bool foundSentenceDelimiter = false;
	bool foundWordDelimiter = false;
	DWORD maxWaitingTimeLimit = 20000;
	DWORD timeExtension = 7000;
	DWORD maxCurrentParticipantInactiveLimit = 7000;

	DWORD maxLabelNameSize = 12;
	DWORD maxLabelSize = maxLabelNameSize+WLEN(LABEL_START)+WLEN(LABEL_END);

	//Check all writters
	for (Writters::iterator it=writters.begin();it!=writters.end();++it)
	{
		//Get writter id
		DWORD id = it->first;
		//Get text writter
		TextWritter *text = it->second;

		//If it is not the active one
		if (text!=currentWritter)
		{
			//Check if it has something in the queue
			if (text->textQueue.length())
			{
				
				//Check if it is not a realtime user and needs to be sent inmediatelly
				if (!text->realtime) {
					//We are the next ones
					nextCurrentParticipant = text;
					//Switch inmediatelly
					switchParticipant = true;
					//Do not look anymore
					break;
				//Check if it was waiting
				} else if (!isZeroTime(&text->waitingSince)) {
					//Get waiting time
					DWORD waitingTime = getDifTime(&(text->waitingSince))/1000;

					//If it is the maximum time in the queue
					if (waitingTime>maxWaitingTime)
					{
						//Set as maximum waiting time
						maxWaitingTime = waitingTime;
						//Set text as current participant candidate
						nextCurrentParticipant = text;
					}
				} else {
					//Waiting since now
					getUpdDifTime(&text->waitingSince);
				}

				//If there are no next participant
				if (!nextCurrentParticipant)
					//We are the next ones
					nextCurrentParticipant = text;
			}
		}
	}

	//Check if waiting time for participant has exceeded the limit
	if (maxWaitingTime>maxWaitingTimeLimit+timeExtension)
		//Switch inmediatelly
		switchParticipant = true;
	else if (maxWaitingTime>maxWaitingTimeLimit)
		//Wait for next to switch
		searchWordDelimiter = true;

	//Check if we have a current participant
	if (currentWritter)
	{
		//Get queue length
		DWORD len = currentWritter->textQueue.length();

		//Check size
		if (len>size-maxLabelSize)
			//limit it
			len = size-maxLabelSize;
		
		//Check that we have length
		if (len)
		{
			//Read text
			currentWritter->textQueue.peek(buffer,len);

			//If we have to search for a word end
			if (searchWordDelimiter)
			{
				///Search for last sentence delimiter
				for (int i=len-1;i>=0;--i)
				{
					//Check character for an space
					if (buffer[i]==' ')
					{
						//End here
						len = i;
						//Found word
						foundWordDelimiter = true;
						//Exit
						break;
					}
				}
			}

			//If we have to search for a sentence end
			if (searchSentenceDelimiter)
			{
				//Check if previous char was an ?! or .
				if (currentWritter->firstSentenceDelimiterCharFound)
				{
					//Check second one on first char
					if (buffer[0]==' ')
					{
						//End here
						len = 1;
						//Found sentence
						foundSentenceDelimiter = true;
					}
				} else {
					//Search for last sentence delimiter
					for (int i=len-2;i>=0;--i)
					{
						//Check character for ?.! followed by space
						if ((buffer[i]=='?' || buffer[i]=='!' ||buffer[i]=='.') &&(buffer[i+1]==' '))
						{
							//End here
							len = i+1;
							//Found sentence
							foundSentenceDelimiter = true;
							//Exit
							break;
						}
					}
				}
			}

			//Check if last char is the first of a sentece delimiter
			if (len>0 && (buffer[len-1]=='?' || buffer[len-1]=='!' ||buffer[len-1]=='.'))
				//Set to true
				currentWritter->firstSentenceDelimiterCharFound = true;
			else
				//Set to false
				currentWritter->firstSentenceDelimiterCharFound = false;

			//If we have to search for a message end
			if (searchMessageDelimiter)
			{
				//Search for last sentence delimiter
				for (int i=0;i<len;i++)
				{
					//Check for message delimiters
					if ((WLEN(LINE_SEPARATOR)<=len-i) && (wmemcmp(buffer+i,LINE_SEPARATOR,WLEN(LINE_SEPARATOR))==0))
					{
						//End here without delimiter
						len = i+WLEN(LINE_SEPARATOR);
						//Found sentence
						foundMessageDelimiter = true;
						//Exit
						break;
					} else if ((WLEN(PARAGRAPH_SEPARATOR)<=len-i) && (wmemcmp(buffer+i,PARAGRAPH_SEPARATOR,WLEN(PARAGRAPH_SEPARATOR))==0)) {
						//End here without delimiter
						len = i+WLEN(PARAGRAPH_SEPARATOR);
						//Found sentence
						foundMessageDelimiter = true;
						//Exit
						break;
					} else if ((WLEN(CRLF)<=len-i) && (wmemcmp(buffer+i,CRLF,WLEN(CRLF))==0)) {
						//End here without delimiter
						len = i+WLEN(CRLF);
						//Found sentence
						foundMessageDelimiter = true;
						//Exit
						break;
					} else if ((WLEN(LF)<=len-i) && (wmemcmp(buffer+i,LF,WLEN(LF))==0)) {
						//End here with	out delimiter
						len = i+WLEN(LF);
						//Found sentence
						foundMessageDelimiter = true;
						//Exit
						break;
					}
				}
			}

			//Remove text from participant queue
			currentWritter->textQueue.remove(len);

			//Send to all readers
			for (Readers::iterator it=readers.begin();it!=readers.end();++it)
			{
				//Get text reader
				TextReader *reader = it->second;

				//NUmber of chars to send
				DWORD send = 0;
				DWORD i = 0;

				//If it is first
				if (reader->isFirst)
				{
					//Reset SGR
					//text->input->WriteText(SGR0,WLEN(SGR0));

					//Append label text start
					wmemcpy(aux+send,LABEL_START,WLEN(LABEL_START));
					//Inc size
					send += WLEN(LABEL_START);

					//Get name length
					DWORD nameLength = currentWritter->name.length();
					//Check size
					if (nameLength>maxLabelNameSize)
						//Cut it
						nameLength = maxLabelNameSize;
					//Append label name
					wmemcpy(aux+send,currentWritter->name.c_str(),nameLength);
					//Inc size
					send += nameLength;

					//Append label text end
					wmemcpy(aux+send,LABEL_END,WLEN(LABEL_END));
					//Inc size
					send += WLEN(LABEL_END);

					//It is not first anymore
					reader->isFirst = 0;
					//No text displayed so far
					reader->displayed = 0;
				}
				
				//Procces text to send
				while(i<len)
				{
					//Depending on the characted
					if ((WLEN(BOM)<=len-i) && (wmemcmp(buffer+i,BOM,WLEN(BOM))==0))
					{	//Skip this one
						i+=WLEN(BOM);
					}
					else if ((WLEN(BACKSPACE)<=len-i) && (wmemcmp(buffer+i,BACKSPACE,WLEN(BACKSPACE))==0))
					{
						//Check number displayed
						if (reader->displayed)
						{
							//Add it
							wmemcpy(aux+send,BACKSPACE,WLEN(BACKSPACE));
							//Increase sent
							send += WLEN(BACKSPACE);
							//Increase counter
							i += WLEN(BACKSPACE);
							//Decreased display counter
							reader->displayed--;
						} else {
							//Put replacement
							wmemcpy(aux+send,ERASURE_REPL,WLEN(ERASURE_REPL));
							//Increase sent
							send += WLEN(ERASURE_REPL);
							//Increase counter
							i += WLEN(BACKSPACE);
						}
					}
					else if ((WLEN(BEL)<=len-i) && (wmemcmp(buffer+i,BEL,WLEN(BEL))==0))
					{
						//Add it 
						wmemcpy(aux+send,BEL,WLEN(BEL));
						//Sent but do not increase displayed
						send += WLEN(BEL);
						//Increase counter but not the displayed count
						i += WLEN(BEL);
					}
					else
					{
						//Add it
						aux[send++] = buffer[i++];
						//One character displayed more;
						reader->displayed++;
					}
				}
				//Text in the buffer is transmitted
				reader->input->WriteText(aux,send);
			}
			//Wait from here
			getUpdDifTime(&currentWritter->waitingSince);

			//If the writer was not realtime
			if (!currentWritter->realtime)
				//Force switch
				switchParticipant = true;
		} else {
			//Get time since last activity
			DWORD inactivity = getDifTime(&currentWritter->waitingSince)/1000;
			//Check if we have exceeded the limit without input
			if (inactivity>maxCurrentParticipantInactiveLimit)
				//We have to switch
				switchParticipant = true;
		}
	} else {
		//We have to switch
		switchParticipant = true;
	}

	//Check if we have to switch or any active delimiter have been found
	if ((switchParticipant || foundMessageDelimiter || foundSentenceDelimiter || foundWordDelimiter) && (currentWritter!=nextCurrentParticipant))
	{
		//Check if there was a current participant
		if (currentWritter)
		{
			//Check if next is not null or participant is a non realtime user (forcing \r\n[name] on next)
			if (nextCurrentParticipant || !currentWritter->realtime)
			{
				//Reset all readers
				for (Readers::iterator it=readers.begin();it!=readers.end();++it)
				{
					//Get text source
					TextReader *reader = it->second;
					//If last is not a message delimiter
					if (!foundMessageDelimiter)
						//Send line separator
						reader->input->WriteText(LINE_SEPARATOR,WLEN(LINE_SEPARATOR));
					//Next is first
					reader->isFirst = 1;
					//Nothing is displayed
					reader->displayed = 0;
				}
				//The writer is not waiting
				setZeroTime(&currentWritter->waitingSince);
				//Change participant anyway
				currentWritter = nextCurrentParticipant;
			}
		} else {	
			//Change participant anyway
			currentWritter = nextCurrentParticipant;
		}
	}

	return 1;
}


int TextMixerWorker::FlushText()
{
	wchar_t buffer[1024];
	wchar_t aux[1024];
	DWORD size=1024;

	DWORD maxLabelNameSize = 12;
	DWORD maxLabelSize = maxLabelNameSize+WLEN(LABEL_START)+WLEN(LABEL_END);

	//While there is somthing left
	while (currentWritter)
	{
		//Get queue length
		DWORD len = currentWritter->textQueue.length();

		//Check size
		if (len>size-maxLabelSize)
			//limit it
			len = size-maxLabelSize;

		//Check that we have length
		if (len)
		{
			//Read text
			currentWritter->textQueue.peek(buffer,len);

			//Remove text from participant queue
			currentWritter->textQueue.remove(len);

			//Send to all readers
			for (Readers::iterator it=readers.begin();it!=readers.end();++it)
			{
				//Get text reader
				TextReader *reader = it->second;

				//NUmber of chars to send
				DWORD send = 0;
				DWORD i = 0;

				//If it is first
				if (reader->isFirst)
				{
					//Reset SGR
					//text->input->WriteText(SGR0,WLEN(SGR0));

					//Append label text start
					wmemcpy(aux+send,LABEL_START,WLEN(LABEL_START));
					//Inc size
					send += WLEN(LABEL_START);

					//Get name length
					DWORD nameLength = currentWritter->name.length();
					//Check size
					if (nameLength>maxLabelNameSize)
						//Cut it
						nameLength = maxLabelNameSize;
					//Append label name
					wmemcpy(aux+send,currentWritter->name.c_str(),nameLength);
					//Inc size
					send += nameLength;

					//Append label text end
					wmemcpy(aux+send,LABEL_END,WLEN(LABEL_END));
					//Inc size
					send += WLEN(LABEL_END);

					//It is not first anymore
					reader->isFirst = 0;
					//No text displayed so far
					reader->displayed = 0;
				}

				//Procces text to send
				while(i<len)
				{
					//Depending on the characted
					if ((WLEN(BOM)<=len-i) && (wmemcmp(buffer+i,BOM,WLEN(BOM))==0))
					{	//Skip this one
						i+=WLEN(BOM);
					}
					else if ((WLEN(BACKSPACE)<=len-i) && (wmemcmp(buffer+i,BACKSPACE,WLEN(BACKSPACE))==0))
					{
						//Check number displayed
						if (reader->displayed)
						{
							//Add it
							wmemcpy(aux+send,BACKSPACE,WLEN(BACKSPACE));
							//Increase sent
							send += WLEN(BACKSPACE);
							//Increase counter
							i += WLEN(BACKSPACE);
							//Decreased display counter
							reader->displayed--;
						} else {
							//Put replacement
							wmemcpy(aux+send,ERASURE_REPL,WLEN(ERASURE_REPL));
							//Increase sent
							send += WLEN(ERASURE_REPL);
							//Increase counter
							i += WLEN(BACKSPACE);
						}
					}
					else if ((WLEN(BEL)<=len-i) && (wmemcmp(buffer+i,BEL,WLEN(BEL))==0))
					{
						//Add it
						wmemcpy(aux+send,BEL,WLEN(BEL));
						//Sent but do not increase displayed
						send += WLEN(BEL);
						//Increase counter but not the displayed count
						i += WLEN(BEL);
					}
					else
					{
						//Add it
						aux[send++] = buffer[i++];
						//One character displayed more;
						reader->displayed++;
					}
				}
				//Text in the buffer is transmitted
				reader->input->WriteText(aux,send);
				//Send line separator
				reader->input->WriteText(LINE_SEPARATOR,WLEN(LINE_SEPARATOR));
				//Next is first
				reader->isFirst = 1;
				//Nothing is displayed
				reader->displayed = 0;
			}
		}

		QWORD maxWaitingTime = 0;
		//Reset writter
		currentWritter = NULL;

		//Check all writters
		for (Writters::iterator it=writters.begin();it!=writters.end();++it)
		{
			//Get text writter
			TextWritter *text = it->second;

			//Check if it has something in the queue
			if (text->textQueue.length())
			{
				//If there are no next participant
				if (!currentWritter)  {
					//We are the next ones
					currentWritter = text;
				} if (!isZeroTime(&text->waitingSince)) {
					//Get waiting time
					DWORD waitingTime = getDifTime(&(text->waitingSince))/1000;

					//If it is the maximum time in the queue
					if (waitingTime>maxWaitingTime)
					{
						//Set as maximum waiting time
						maxWaitingTime = waitingTime;
						//Set text as current participant candidate
						currentWritter = text;
					}
				}
			}
		}
	}

	return 1;
}
