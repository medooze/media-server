#ifndef TEXTENCODER_H_
#define	TEXTENCODER_H_
#include "text.h"
#include <set>

class TextEncoder
{
public:
	TextEncoder();
	~TextEncoder();

	int Init(TextInput *input);
	bool AddListener(MediaFrame::Listener *listener);
	bool RemoveListener(MediaFrame::Listener *listener);
	int StartEncoding();
	int StopEncoding();
	int End();

	int IsEncoding() { return encodingText;}

protected:
	int Encode();


private:
	//Funciones propias
	static void *startEncoding(void *par);

private:
	typedef std::set<MediaFrame::Listener*> Listeners;
	
private:
	Listeners		listeners;
	TextInput*		textInput;
	pthread_mutex_t		mutex;
	pthread_t		encodingTextThread;
	int			encodingText;
};

#endif	/* TEXTENCODER_H */

