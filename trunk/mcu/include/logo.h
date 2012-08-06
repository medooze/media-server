#ifndef _LOGO_H_
#define _LOGO_H_
#include "config.h"

class Logo
{
public:
	Logo();
	~Logo();
	int Load(const char *filename);

	BYTE* GetFrame();
	int GetWidth();
	int GetHeight();

private:
	BYTE*	 frame;
	int width;
	int height;
};

#endif
