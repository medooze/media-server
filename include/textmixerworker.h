#ifndef _TEXMIXERWORKER_H_
#define	_TEXMIXERWORKER_H_
#include "config.h"
#include "text.h"
#include "pipetextinput.h"
#include "fifo.h"
#include <string>

class TextMixerWorker
{
public:
	TextMixerWorker();
	~TextMixerWorker();
	int Init();
	int AddWritter(DWORD id, const std::wstring name,bool realtime);
	int WriteText(DWORD id,const wchar_t *data,DWORD size);
	int ProcessText();
	int FlushText();
	int RemoveWritter(DWORD id);
	int AddReader(DWORD id,PipeTextInput *input);
	int RemoveReader(DWORD id);
	int End();
	
private:
	//Tipos
	struct TextWritter
	{
		DWORD			id;
		std::wstring		name;
		bool			realtime;
		fifo<wchar_t,1024>	textQueue;
		timeval			waitingSince;
		bool			firstSentenceDelimiterCharFound;

		TextWritter(DWORD id,std::wstring name,bool realtime)
		{
			this->id = id;
			this->name = name;
			this->realtime = realtime;
			//Set zero time
			setZeroTime(&waitingSince);
			//Not found
			firstSentenceDelimiterCharFound = false;
		}
	};

	struct TextReader
	{
		DWORD	id;
		int	isFirst;
		DWORD	displayed;
		PipeTextInput *input;

		TextReader(DWORD id,PipeTextInput *input)
		{
			this->id = id;
			this->input = input;
			//Set zero values
			isFirst = 1;
			displayed = 0;
		}
	};

	typedef std::map<DWORD,TextWritter*> Writters;
	typedef std::map<DWORD,TextReader*> Readers;
private:
	Writters	writters;
	Readers		readers;
	TextWritter	*currentWritter;
};

#endif	/* _TEXMIXERWORKER_H_ */

