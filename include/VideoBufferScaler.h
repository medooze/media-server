#ifndef _FRAMESCALER_H_
#define _FRAMESCALER_H_
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <config.h>
#include <video.h>

class VideoBufferScaler
{
public:
	int Resize(const VideoBuffer::const_shared& input, const VideoBuffer::shared& output, bool keepAspectRatio = true);
};

#endif
