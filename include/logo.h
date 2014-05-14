#ifndef _LOGO_H_
#define _LOGO_H_
#include "config.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

class Logo
{
public:
	Logo();
	~Logo();
	int Load(const char *filename);

	BYTE* GetFrame() const;
	BYTE* GetFrameRGBA() const;
	int GetWidth() const;
	int GetHeight() const;

private:
	BYTE*	 frame;
	int width;
	int height;
	AVFrame *logoRGB;
};

#endif
